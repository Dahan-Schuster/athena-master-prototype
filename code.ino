// C++ code

/************************************************************/
/*                         INCLUDES                         */
/************************************************************/

// TODO: revisar bibliotecas necessárias

#include <Metro.h>                // biblioteca para facilitar multitasking

#include <SPI.h>                  // protocolo de comunicação SPI
#include <Wire.h>                 // protocolo de comunicação 
#include <Adafruit_GFX.h>         // display oled
#include <Adafruit_SSD1306.h>     // display oled

#include <DHT.h>                  // DHT11

/*************************************************************/
/*                         DIRETIVAS                         */
/*************************************************************/

/* FIXME: pinos meramente ilustrativos enquanto não houver diagramação **/

/* Entradas */

// entrada digital
#define BTN_INICIAR       33
#define BTN_TOVIVO        35
#define BTN_RESFRIAMENTO  32
#define BTN_FALA          25

// entrada analógica
#define HALL_ACELERACAO   5
#define HALL_FRENAGEM     17
#define HALL_ROTACAO      16
#define HALL_VELOCIDADE   18

#define SENSOR_TEMP       14
#define DHT_TYPE          DHT11

/* Saídas */

// saída digital
#define LED_BTN_FALA      0

#define LED_ERRO_INTERNO  0

#define LED_INATIVIDADE   0
#define BUZZER            0


// saída analógica
#define COOLER            0

#define PLAYER            0

#define OLED_SCL          22
#define OLED_SDA          21
#define OLED_RESET        -1 // Reset pin # (or -1 if sharing Arduino reset pin)

/* Constantes */

#define TEMP_MAX          30 // °C
#define HUMD_MAX          90 // % 

#define VELOCIDADE_COOLER 100

// tempo de checagem de inatividade do motorista
#define DELAY_SEGURANCA   60000 * 5 // 300000 ms = 5 min

// tamanho da tela do display oled
#define SCREEN_WIDTH      128 // px
#define SCREEN_HEIGHT      64 // px

// códigos de erro interno, representando o número de beeps a tocar
#define CODIGO_ERRO_DISPLAY 3 // beeps
#define CODIGO_ERRO_DHT     5

/************************************************************/
/*                    VARIÁVEIS GLOBAIS                     */
/************************************************************/

// objetos utilizados para controlar tarefas paralelas
// define um intervalo em ms para execução de processos
Metro taskBotaoMicrofone = Metro(500);
Metro taskBotaoCooler = Metro(500);
Metro taskAtualizarVelocidade = Metro(50);
Metro taskChecarTemperatura = Metro(100);
Metro taskSeguranca = Metro(DELAY_SEGURANCA);

// Display oled
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

DHT dht(SENSOR_TEMP, DHT_TYPE);

// variáveis de estado (guardam o valor dos sensores)
int isSistemaLigado = 0;

int isCoolerAtivo = 0;

int isMicrofoneAtivo = 0;

int isMotoristaVivo = 1;

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
// FIXME: revisar configuração dos pinos
void configurarPinos() {
  pinMode(BTN_INICIAR, INPUT);
  pinMode(BTN_TOVIVO, INPUT);
  pinMode(BTN_FALA, INPUT);
  pinMode(BTN_RESFRIAMENTO, INPUT);
  pinMode(HALL_ACELERACAO, INPUT);
  pinMode(HALL_FRENAGEM, INPUT);
  pinMode(HALL_ROTACAO, INPUT);
  pinMode(HALL_VELOCIDADE, INPUT);
  pinMode(SENSOR_TEMP, INPUT);

  pinMode(LED_BTN_FALA, OUTPUT);
  pinMode(LED_ERRO_INTERNO, OUTPUT);
  pinMode(LED_INATIVIDADE, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(COOLER, OUTPUT);
  pinMode(PLAYER, OUTPUT);

  digitalWrite(LED_ERRO_INTERNO, LOW);

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
 */
int lerEstadoBotao(int pin) {
  int estado = digitalRead(pin);
  
  // se o botão foi pressionado, suspende
  // o programa até que seja solto
  while (digitalRead(pin) == HIGH) {
    // FIXME: melhorar o método para não usar delay
    delay(100);
  }
  
  return estado;
}

/**
 * Lê o valor de um pino como analógico e o retorna convertido para digital
 */
float getValorAnalogicoDigital(int pin) {
  // Lê o valor analógico do potenciômetro,
  // que varia entre 0 e 1023
  int valorAnalogico = analogRead(pin);

  // Mapeia o valor analógico para digital
  return valorAnalogico * 5 / 1023.0;
}

// TODO: leitura do microfone
int captarVozMicrofone() {
}

/** Envia o comando de desligar a todos os componentes */
// TODO: revisar componentes que precisam desligar
void desligarComponentes() {

}


/************************************************************/
/*                    FUNÇÕES DE COMANDO                    */
/*                    -------------------                   */
/*           Determinam a lógica de funcionamento           */
/************************************************************/

/**
 * Faz a leitura dos sensores de velocidade e atualiza as variáveis
 */
void atualizarVelocidade() {
  // verifica se é hora de checar os sensores de velocidade
  if (taskAtualizarVelocidade.check()) {
    velocidadeAtual = getValorAnalogicoDigital(HALL_VELOCIDADE);
    rotacaoAtual = getValorAnalogicoDigital(HALL_ROTACAO);
    aceleracaoAtual = getValorAnalogicoDigital(HALL_ACELERACAO);
    frenagemAtual = getValorAnalogicoDigital(HALL_FRENAGEM);
  }
}

/**
 * Faz a leitura dos sensores de temperatura e liga/desliga o cooler de acordo
 */
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

/** Verifica sinais vitais do motorista e dispara o alerta de inatividade */
// FIXME:
// Colocar um aviso prévio (buzzer, led)
// Colocar outro botão de segurança por backup
void checarVidaMotorista() {
  // verifica se é hora de checar se o motorista está vivo
  if (taskSeguranca.check()) {
    int isMotoristaVivo = lerEstadoBotao(BTN_TOVIVO);

    if (!isMotoristaVivo)
      alertarInatividade();
  }
}

// TODO: alerta de inatividade do motorista
// Colocar um aviso prévio (buzzer, led)
void alertarInatividade() {
  
}

/**
 * Inicia os processos do módulo Segurança
 */
void checarSeguranca() {
  checarVidaMotorista();
  checarTemperatura();
  checarBotaoCooler();
}

/** Apresenta as variáveis de controle no display oled */
void mostrarDadosNoDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  // cooler ativo
  // microfone ativo
  // aceleração
  // velocidade
  // rotação
  // frenagem

  // temperatura
  display.setTextSize(1);
  display.setCursor(1,1);
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
  
  display.display(); 
}

/** Executa os processos paralelos do sistema */
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
      digitalWrite(LED_ERRO_INTERNO, HIGH);
      tone(BUZZER, 440, 500);
      delay(800);
      noTone(BUZZER);
      digitalWrite(LED_ERRO_INTERNO, LOW);
    }

    delay(1250);
  }
}

/************************************************************/
/*              FUNÇÕES DE CONTROLE DE ENERGIA              */
/*              ------------------------------              */
/*      Controlam o estado de funcionamento do sistema      */
/************************************************************/

/**
 * Redefine as variáveis e chama a
 * função que desliga os componentes
 */
void desligar() {
  isSistemaLigado = 0;   // desliga logicamente o sistema
  desligarComponentes(); // desliga todos os componentes
}

/** Liga logicamente o sistema */
void ligar() {
  isSistemaLigado = 1;   // liga logicamente o sistema
  executarProcessos();   // inicia todos os processos
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