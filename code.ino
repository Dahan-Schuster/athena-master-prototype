// C++ code

/************************************************************/
/*                         INCLUDES                         */
/************************************************************/

// TODO: revisar bibliotecas necessárias

/*************************************************************/
/*                         DIRETIVAS                         */
/*************************************************************/

/* FIXME: pinos meramente ilustrativos enquanto não houver diagramação **/

/* Entradas */

// entrada digital
#define BTN_INICIAR 1
#define BTN_TOVIVO 2
#define BTN_RESFRIAMENTO 3
#define BTN_FALA 4

// entrada analógica
#define HALL_ACELERACAO A1
#define HALL_FRENAGEM A2
#define HALL_ROTACAO A3
#define HALL_VELOCIDADE A4

#define SENSOR_TEMP A5

#define MICROFONE A6

/* Saídas */

// saída digital
#define LED_BTN_FALA 1

// saída analógica
#define COOLER A7

#define PLAYER A8

#define DISPLAY_OLED A9

/* Constantes */

// tempo de espera até verificar novamente se o piloto
// está vivo após a última confirmação
#define DELAY_SEGURANCA 60000 * 5 // 300000 ms = 5 min

// tempo de espera da confirmação do motorista
#define TIMEOUT_SEGURANCA  20000 // 20 s

#define TEMP_MAX 30 // °C
#define TEMP_MIN 20 // °C

#define VELOCIDADE_MAX_COOLER 150
#define VELOCIDADE_PADRAO_COOLER 100
#define VELOCIDADE_MIN_COOLER 50

// tempo de checagem de cada tarefa paralela
#define DELAY_MICROFONE 500
#define DELAY_VELOCIDADE 50
#define DELAY_TEMPERATURA 2000

/************************************************************/
/*                    VARIÁVEIS GLOBAIS                     */
/************************************************************/

// timestamps utilizados para controlar tarefas paralelas
unsigned long ts_microfone = millis();
unsigned long ts_velocidade = millis();
unsigned long ts_temperatura = millis();
unsigned long ts_seguranca = millis();
unsigned long ts_motorista_vivo = millis();

// variáveis de estado (guardam o valor dos sensores)
int isSistemaLigado = 0;

int isCoolerAtivo = 0;

int isMicrofoneAtivo = 0;

int isMotoristaVivo = 1;

int temperaturaAtual = 0;

int aceleracaoAtual = 0;

int frenagemAtual = 0;

int velocidadeAtual = 0;

int rotacaoAtual = 0;

int velocidadeCooler = 0;

/************************************************************/
/*                FUNÇÕES DE BAIXO NÍVEL                    */
/*             -----------------------------                */
/*             Controlam e ditam a lógica de                */
/*             funcionamento dos componentes                */
/************************************************************/

/** Configura a utilização dos pinos do Arduino */
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
  pinMode(MICROFONE, INPUT);

  pinMode(LED_BTN_FALA, OUTPUT);
  pinMode(COOLER, OUTPUT);
  pinMode(PLAYER, OUTPUT);
  pinMode(DISPLAY_OLED, OUTPUT);
}


/* ====================================
 * Funções que guardam a lógica de
 * ativação/desativação dos componentes
 * ==================================== */

void ligarLed(int pin) { 
  digitalWrite(pin, HIGH);
}

void desligarLed(int pin) {
  digitalWrite(pin, LOW);
}

void ligarCooler(int velocidade) {
  analogWrite(COOLER, velocidade);
}

void desligarCooler() {
  analogWrite(COOLER, LOW);
}

/* ====================================
 * Funções que guardam a lógica de
 * ativação/desativação dos componentes
 * ==================================== */

/**
 * Lê o estado de um push button, recebendo seu pin como parâmetro
 * Impede de ler mais de um valor caso o botão continue pressionado
 */
int lerEstadoBotao(int pin) {
  int estado = digitalRead(pin);
  
  // se o botão foi pressionado, suspende
  // o programa até que seja solto
  while (digitalRead(pin) == HIGH) {
    // espera 0.1s antes de verificar o estado de novo
    delay(100);
  }
  
  return estado;
}

/**
 * Lê o valor de um pino como analógico e o retorna convertido para digital
 */
int getValorAnalogicoDigital(int pin) {
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
  if (millis() - ts_velocidade >= DELAY_VELOCIDADE) {

    // atualiza o timestamp
    ts_velocidade += DELAY_VELOCIDADE;
  	
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
  if (millis() - ts_temperatura >= DELAY_TEMPERATURA) {
    ts_temperatura += DELAY_TEMPERATURA;

    temperaturaAtual = analogRead(SENSOR_TEMP);

    // verifica se a temperatura excedeu o limite máximo
    if (temperaturaAtual >= TEMP_MAX) {
      // aumenta gradativamente a velocidade do cooler
      if (velocidadeCooler <= VELOCIDADE_MAX_COOLER) velocidadeCooler += 20;

      ligarCooler(velocidadeCooler);
    } else if (temperaturaAtual <= TEMP_MIN) {
      if (velocidadeCooler > VELOCIDADE_MIN_COOLER) velocidadeCooler -= 20;

      ligarCooler(velocidadeCooler);
    } else {
      desligarCooler();
    }
  }
}

/** Verifica sinais vitais do motorista e dispara o alerta de inatividade */
// FIXME: consultar comportamento esperado do sistema
void checarVidaMotorista() {
  // verifica se é hora de checar se o motorista está vivo
  if (millis() - ts_motorista_vivo >= DELAY_SEGURANCA) {
    int isMotoristaVivo = lerEstadoBotao(BTN_TOVIVO);
    ts_motorista_vivo += isMotoristaVivo ? DELAY_SEGURANCA : TIMEOUT_SEGURANCA;

  // continuar esperando pelo sinal de vida durante um certo tempo (timeout) ?
  } else if (millis() - ts_motorista_vivo < TIMEOUT_SEGURANCA) {
    alertarInatividade();
  }
}

// TODO: alerta de inatividade do motorista
void alertarInatividade() {
}

/** Verifica se o botão de resfriamento foi acionado */
void checarBotaoCooler() {
  // Checa se o botão Cooler foi pressionado
  int isBotaoCoolerOn = lerEstadoBotao(BTN_RESFRIAMENTO);

  if (isBotaoCoolerOn)
    isCoolerAtivo = !isCoolerAtivo;

  if (isCoolerAtivo)
    ligarCooler(VELOCIDADE_PADRAO_COOLER);
  else
    desligarCooler();
}

void checarSeguranca() {
  checarVidaMotorista();
  checarBotaoCooler();

  if (!isCoolerAtivo) {
    checarTemperatura();
  }
}

/** Executa os processos paralelos do sistema */
void executarProcessos()
{
  atualizarVelocidade();
  checarSeguranca();
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
void desligar()
{
  isSistemaLigado = 0;   // desliga logicamente o sistema
  desligarComponentes(); // desliga todos os componentes
}

/** Liga logicamente o sistema */
void ligar()
{
  isSistemaLigado = 1;   // liga logicamente o sistema
  executarProcessos();   // inicia todos os processos
}

/** Inverte o estado do sistema */
void reinicar()
{
  if (isSistemaLigado) {
    desligar();  
  } else {
    ligar(); 
  }
}

/************************************************************/
/*                       SETUP E LOOP                       */
/************************************************************/

void setup()
{
  configurarPinos();
}

/**
 * A função loop é responsável por verificar o estad do botão
 * Inicar e, a partir disso, ditar o rumo do programa
 */
void loop()
{ 
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
  
  delay(10); // um pouco de delay para melhorar a performance
}