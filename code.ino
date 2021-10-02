// C++ code

/************************************************************/
/*                         INCLUDES                         */
/************************************************************/

#include <Metro.h>                // biblioteca para facilitar multitasking

#include <SPI.h>                  // protocolo de comunicação SPI (display oled)
#include <Wire.h>                 // protocolo de comunicação I2C (display oled)
#include <Adafruit_GFX.h>         // display oled
#include <Adafruit_SSD1306.h>     // display oled

#include <DHT.h>                  // DHT11

/*************************************************************/
/*                         DIRETIVAS                         */
/*************************************************************/

/* Entradas e saídas */

// botão de ligar/desligar o sistema
#define BTN_INICIAR       16

#define HALL_ACELERACAO   39
#define HALL_FRENAGEM     36
#define HALL_ROTACAO      34
#define HALL_VELOCIDADE   35

#define SENSOR_DHT        13

#define BTN_RESFRIAMENTO  15
#define COOLER            19

#define LED_BTN_FALA      0
#define BTN_FALA          0
#define PLAYER            0

#define OLED_SCL          22 // pino I2C SCL
#define OLED_SDA          21 // pino I2C SDA
#define OLED_RESET        -1 // pino reset (-1 caso o display não possua)


// botões de confirmação de vida
#define BTN_CONFIRMAR_VIDA          26
#define BTN_CONFIRMAR_VIDA_BACKUP   27

// leds de alerta
#define LED_ALERTA_CONFIRMACAO      14
#define LED_ALERTA_INATIVIDADE      12
#define LED_ALERTA_ERRO_INTERNO     2

// buzzer para alertas gerais
#define BUZZER                      4

/* Constantes */

#define DHT_TYPE          DHT11

#define TEMP_MAX          30 // °C
#define HUMD_MAX          90 // % 

#define VELOCIDADE_COOLER 100

// tamanho da tela do display oled
#define SCREEN_WIDTH      128 // px
#define SCREEN_HEIGHT      64 // px

// códigos de erro interno, representando o número de beeps a tocar
#define CODIGO_ERRO_DISPLAY 3 // beeps
#define CODIGO_ERRO_DHT     5


/** 
 * DIRETIVAS DE CHECAGEM DE VIDA DO MOTORISTA
 * ------------------------------------------
 * A cada 4min30s é iniciado o alerta de confirmação
 * de vida pendente. Durante os próximos 30s, um alerta
 * é emitido. Passados 5 min ao total e o botão de confirmação
 * não tiver sido pressionado, todo o sistema é interrompido
 */

// intervalo de checagem de inatividade do motorista
#define SYSTEM_INTERRUPT_TIMEOUT    1000 * 60 * 5   // 5 min

// intervalo entre os alertas de checagem de inatividade
#define LIFE_CHECK_ALERT_DELAY      1000 * 60 * 4.5 // 4min30s

// intervalo entre os beeps de alerta da próxima checagem
#define LIFE_CHECK_ALERT_BEEP_DELAY 300

/**
 * FIM DIRETIVAS DE CHECAGEM DE VIDA DO MOTORISTA
 * ----------------------------------------------
 */

/************************************************************/
/*                    VARIÁVEIS GLOBAIS                     */
/************************************************************/

// objetos utilizados para controlar tarefas paralelas
// define um intervalo em ms para execução de processos
Metro taskBotaoMicrofone = Metro(500);        // 0.5 s
Metro taskBotaoCooler = Metro(500);           // 0.5 s
Metro taskAtualizarVelocidade = Metro(50);    // 50 ms
Metro taskChecarTemperatura = Metro(1000);    // 1 s
Metro taskMostrarDadosDisplay = Metro(200);   // 0.2 s

// timestamp utilizado para controle do alerta de checagem de vida
// escolhido ao invés do Metro para obter maior controle sobre a tarefa
unsigned long int tsLifeCheckAlert = millis();
unsigned long int tsToggleAlert = millis();
unsigned long int tsSystemInterruptTimeout = millis();

// Display oled
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// array bitmap do ícone de cooler mostrado no display
static const PROGMEM uint8_t coolerIcon[16 * 16 / 8] = { 
  0x03, 0x00, 0x0F, 0x10, 0x0F, 0x3C, 0x0F, 0x7E, 0x47, 0x7C, 0xF7, 0xF0, 0xFF, 0xE0, 0xFE, 0x7E,
  0x7E, 0x7F, 0x07, 0xFF, 0x0F, 0xEF, 0x3E, 0xE2, 0x7E, 0xF0, 0x3C, 0xF0, 0x08, 0xF0, 0x00, 0xC0
};

// array bitmap do ícone de microfone mostrado no display
static const PROGMEM uint8_t micIcon[16 * 16 / 8] = { 
  0x1C, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0xBE, 0x80,
  0xBE, 0x80, 0xBE, 0x80, 0xDD, 0x80, 0x63, 0x00, 0x3E, 0x00, 0x08, 0x00, 0x08, 0x00, 0x3E, 0x00
};

// array bitmap do ícone de coração (confirmação de vida pendente) mostrado no display
static const PROGMEM uint8_t lifeIcon[16 * 16 / 8] = { 
  0x1C, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0xBE, 0x80,
  0xBE, 0x80, 0xBE, 0x80, 0xDD, 0x80, 0x63, 0x00, 0x3E, 0x00, 0x08, 0x00, 0x08, 0x00, 0x3E, 0x00
};

// sensor de temperatura e humidade
DHT dht(SENSOR_DHT, DHT_TYPE);

// variáveis de estado (guardam o valor dos sensores)
int isSistemaLigado = 0;

int isCoolerAtivo = 0;

int isMicrofoneAtivo = 0;

int isConfirmacaoVidaPendente = 0;

float temperaturaAtual = 0;

float humidadeAtual = 0;

float aceleracaoAtual = 0;

float frenagemAtual = 0;

float velocidadeAtual = 0;

float rotacaoAtual = 0;

/************************************************************/
/*                FUNÇÕES DE BAIXO NÍVEL                    */
/*             -----------------------------                */
/*             Controlam e ditam a lógica de                */
/*             funcionamento dos componentes                */
/************************************************************/

/** Configura a utilização dos pinos do Arduino */
void configurarPinos() {
  pinMode(BTN_INICIAR, INPUT);

  pinMode(HALL_ACELERACAO, INPUT);
  pinMode(HALL_FRENAGEM, INPUT);
  pinMode(HALL_ROTACAO, INPUT);
  pinMode(HALL_VELOCIDADE, INPUT);

  pinMode(BTN_RESFRIAMENTO, INPUT);
  pinMode(COOLER, OUTPUT);

  // pinMode(LED_BTN_FALA, OUTPUT);
  // pinMode(BTN_FALA, INPUT);
  // pinMode(PLAYER, OUTPUT);

  pinMode(BTN_CONFIRMAR_VIDA, INPUT);
  pinMode(BTN_CONFIRMAR_VIDA_BACKUP, INPUT);

  pinMode(LED_ALERTA_INATIVIDADE, OUTPUT);
  pinMode(LED_ALERTA_CONFIRMACAO, OUTPUT);
  pinMode(LED_ALERTA_ERRO_INTERNO, OUTPUT);
  
  pinMode(BUZZER, OUTPUT);

  dht.begin();
}

void inicializarDisplay() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    alertarErroInterno(CODIGO_ERRO_DISPLAY);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
}

/* ====================================
 * Funções que guardam a lógica de
 * ativação/desativação dos componentes
 * ==================================== */

void ligarCooler() {
  isCoolerAtivo = 1;
  analogWrite(COOLER, VELOCIDADE_PADRAO_COOLER);
}

void desligarCooler() {
  isCoolerAtivo = 0;
  analogWrite(COOLER, LOW);
}

/**
 * Lê o estado de um push button, recebendo seu pin como parâmetro
 * Impede de ler mais de um valor caso o botão continue pressionado
 * FIXME: melhorar o método para não usar delay
 */
int lerEstadoBotao(int pin) {
  int estado = digitalRead(pin);
  
  // se o botão foi pressionado, suspende
  // o programa até que seja solto
  while (digitalRead(pin) == HIGH) {
    delay(100);
  }
  
  return estado;
}

/** Lê o valor de um pino como analógico e o retorna convertido para digital */
float getValorAnalogicoDigital(int pin) {
  // Lê o valor analógico do potenciômetro,
  // que varia entre 0 e 1023
  int valorAnalogico = analogRead(pin);

  // Mapeia o valor analógico para digital
  return valorAnalogico * 5 / 1023.0;
}

/** Envia o comando de desligar a todos os componentes */
void desligarComponentes() {
  desligarCooler();

  isMicrofoneAtivo = 0;
  digitalWrite(LED_BTN_FALA, LOW);

  digitalWrite(LED_ALERTA_ERRO_INTERNO, LOW);
  digitalWrite(LED_ALERTA_CONFIRMACAO, LOW);
  digitalWrite(LED_ALERTA_INATIVIDADE, LOW);

  noTone(BUZZER);

  display.clearDisplay();
}


/************************************************************/
/*                    FUNÇÕES DE COMANDO                    */
/*                    -------------------                   */
/*           Determinam a lógica de funcionamento           */
/************************************************************/


/** Faz a leitura dos sensores de velocidade e atualiza as variáveis */
void atualizarVelocidade() {
  // verifica se é hora de checar os sensores de velocidade
  if (taskAtualizarVelocidade.check()) {
    velocidadeAtual = getValorAnalogicoDigital(HALL_VELOCIDADE);
    rotacaoAtual = getValorAnalogicoDigital(HALL_ROTACAO);
    aceleracaoAtual = getValorAnalogicoDigital(HALL_ACELERACAO);
    frenagemAtual = getValorAnalogicoDigital(HALL_FRENAGEM);
  }
}

/** Faz a leitura dos sensores de temperatura e liga/desliga o cooler de acordo */
void checarTemperatura() {
  if (taskChecarTemperatura.check()) {
    temperaturaAtual = dht.readTemperature();
    humidadeAtual = dht.readHumidity();

    if (isnan(temperaturaAtual) || isnan(humidadeAtual)) {
      alertarErroInterno(CODIGO_ERRO_DHT);
    }

    // se o cooler não já estiver ativo
    if(!isCoolerAtivo) {
      // verifica se a temperatura excedeu o limite máximo
      if (temperaturaAtual >= TEMP_MAX || humidadeAtual >= HUMD_MAX) {
        ligarCooler();
      } else {
        desligarCooler();
      }
    }
  }
}

/** Verifica se o botão de resfriamento foi acionado */
void checarBotaoCooler() {
  if (taskBotaoCooler.check()) {
    // Checa se o botão Cooler foi pressionado
    int isBotaoCoolerOn = lerEstadoBotao(BTN_RESFRIAMENTO);

    // se o botão foi pressionado, troca o estado do cooler
    if (isBotaoCoolerOn)
      isCoolerAtivo = !isCoolerAtivo;

    if (isCoolerAtivo)
      ligarCooler();
    else
      desligarCooler();
  }
}

/** Desliga os componentes que alertam que a confirmação de vida está pendente */
void desligarAlertaConfirmacao() {
  isConfirmacaoVidaPendente = 0;
  digitalWrite(LED_ALERTA_CONFIRMACAO, LOW);
  noTone(BUZZER);
}

/** Beepa repetidamente para alertar que a confirmação de vida está pendente */
void alertarAguardandoConfirmacao() {
  isConfirmacaoVidaPendente = 1;

  if (millis() - tsToggleAlert < LIFE_CHECK_ALERT_BEEP_DELAY) {
    digitalWrite(LED_ALERTA_CONFIRMACAO, HIGH);
    tone(BUZZER, 440, LIFE_CHECK_ALERT_BEEP_DELAY);
  } else {
    digitalWrite(LED_ALERTA_CONFIRMACAO, LOW);
    noTone(BUZZER);
  }
  
  if (millis() - tsToggleAlert > LIFE_CHECK_ALERT_BEEP_DELAY * 2) {
    tsToggleAlert = millis();
  }
}

/** Liga os componentes que alertam a inatividade do motorista */
void alertarInatividade() {
  digitalWrite(LED_ALERTA_INATIVIDADE, HIGH);
  tone(BUZZER, 550);
}

/** Suprime os alertas de inatividade do motorista */
void desligarAlertaInatividade() {
  digitalWrite(13, LOW);
  noTone(BUZZER);
}

/** Verifica sinais vitais do motorista e dispara o alerta de inatividade */
void checarVidaMotorista() {
  // a cada X segundos, começa o alerta de aguardando confirmação
  if (millis() - tsLifeCheckAlert >= LIFE_CHECK_DELAY) {
    // aguarda a confirmação de vida
    int isMotoristaVivo = digitalRead(BTN_CONFIRMAR_VIDA);
    int isMotoristaVivobBackup = digitalRead(BTN_CONFIRMAR_VIDA_BACKUP);

    // quando o motorista confirmar que está vivo...
    if (isMotoristaVivo || isMotoristaVivobBackup) {
      // atualiza os intervalos e desliga os alertas
      tsLifeCheckAlert = millis();
      tsSystemInterruptTimeout = millis();

      desligarAlertaConfirmacao();
      desligarAlertaInatividade();
    }
    
    // se o motorista excedeu o tempo de confirmação...
    else if (millis() - tsSystemInterruptTimeout >= SYSTEM_INTERRUPT_TIMEOUT) {
      desligar();           // encerra os processos do sistema
      alertarInatividade(); // alerta a inatividade do motorista
    }

    // se ainda está no intervalo de espera de confirmação...
    else {
      alertarAguardandoConfirmacao();
    }
  }
}

/** Inicia os processos do módulo Segurança */
void checarSeguranca() {
  checarVidaMotorista();
  checarTemperatura();
  checarBotaoCooler();
}

/** Apresenta as variáveis de controle no display oled */
void mostrarDadosNoDisplay() {
  if (taskMostrarDadosDisplay.check()) {
    display.clearDisplay();
  
    // temperatura
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("Temp ");
    display.write(167);
    display.print("C");
    display.setTextSize(2);
    display.setCursor(0,10);
    display.print(temperaturaAtual);
    
    // humidade
    display.setTextSize(1);
    display.setCursor(65, 0);
    display.print("Hum %");
    display.setTextSize(2);
    display.setCursor(65, 10);
    display.print(humidadeAtual);
    
    // velocidade
    display.setTextSize(1);
    display.setCursor(0, 32);
    display.print("Vel Km/h");
    display.setTextSize(2);
    display.setCursor(0, 42);
    display.print(velocidadeAtual);

    // Ícone microfone
    static int micIconDisplayed = 0;    // usado para fazer o ícone piscar na tela
    if (isMicrofoneAtivo) {
      if (!micIconDisplayed)
        display.drawBitmap(75, 40, micIcon, MIC_ICON_WIDTH, MIC_ICON_HEIGHT, 1);

      micIconDisplayed = !micIconDisplayed;
    }

    // Ícone cooler
    static int coolerIconDisplayed = 0; // usado para fazer o ícone piscar na tela
    if (isCoolerAtivo) {
      if (!coolerIconDisplayed)
        display.drawBitmap(90, 40, coolerIcon, COOLER_ICON_WIDTH, COOLER_ICON_HEIGHT, 1);

      coolerIconDisplayed = !coolerIconDisplayed;
    }

    // Ícone situação vida motorista
    static int lifeIconDisplayed = 0;

    // se estiver pendente, pisca o ícone de coração
    if (isConfirmacaoVidaPendente) {
      if (!lifeIconDisplayed)
        display.drawBitmap(110, 40, lifeIcon, MIC_ICON_WIDTH, MIC_ICON_HEIGHT, 1);

      lifeIconDisplayed = !lifeIconDisplayed;
    }
    
    // se não estiver pendente, mostra o ícone estático
    else {
        display.drawBitmap(110, 40, lifeIcon, 16, 16, 1);
    }

    display.display();
  }
}

/** Executa os processos paralelos do sistema */
// TODO: módulo comunicação
void executarProcessos() {
  atualizarVelocidade();
  checarSeguranca();
  mostrarDadosNoDisplay();
}

/** Desliga o sistema e beepa o código de erro */
void alertarErroInterno(int codigoErro) {
  desligar();
  int i;

  while(true) {
    for (i = 0; i < codigoErro; i++) {
      digitalWrite(LED_ALERTA_ERRO_INTERNO, HIGH);
      tone(BUZZER, 440, 500);
      delay(800);
      noTone(BUZZER);
      digitalWrite(LED_ALERTA_ERRO_INTERNO, LOW);
    }

    delay(1250);
  }
}

/************************************************************/
/*              FUNÇÕES DE CONTROLE DE ENERGIA              */
/*              ------------------------------              */
/*      Controlam o estado de funcionamento do sistema      */
/************************************************************/

/** Desliga logicamente o sistema e chama a função que para os componentes */
void desligar() {
  isSistemaLigado = 0;
  desligarComponentes();
}

/** Liga logicamente o sistema e inicia os processos paralelos */
void ligar() {
  desligarComponentes();
  
  // redefine os timestamps
  tsSystemInterruptTimeout =  millis();
  tsLifeCheckAlert = millis();

  isSistemaLigado = 1;
  executarProcessos();
}

/** Inverte o estado do sistema */
void reinicar() {
  if (isSistemaLigado) {
    desligar();  
  } else {
    ligar(); 
  }
}

/************************************************************/
/*                       SETUP E LOOP                       */
/************************************************************/

void setup() {
  configurarPinos();
  inicializarDisplay();
}

void loop() { 
    // Checa se o botão Iniciar foi pressionado
    int isBotaoIniciarOn = lerEstadoBotao(BTN_INICIAR);
    
    // Se o botão foi pressionado
    if (isBotaoIniciarOn) {
    
      // Inverte o estado do sistema (ligado/desligado)
      reinicar();
    } else {
      
      // Se o botão não foi pressionado, o sistema permanece
      // os processos onde estavam, parados ou executando
      if (isSistemaLigado) {
        executarProcessos();
      }
      
      // não faz nada se estiver desligado ¯\_(ツ)_/¯
    }
}