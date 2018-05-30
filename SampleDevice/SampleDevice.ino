#define BAUD        230400

//#define USE_ARDUINO

#define OUTPUT_PORT digitalPinToPort(13)
// 0x38 = 0x07 << 3

#define PIN(P)      digitalPinToBitMask(P)
#define UPDATE(CMD) OUTPUT_PORT = ((CMD << 3) & OUTPUT_MASK);


#ifdef USE_ARDUINO
uint8_t     PIN11, PIN12, PIN13;
#else
uint8_t     OUTPUT_MASK;
#endif
volatile uint8_t *_outputRegister;
volatile uint8_t *_pinModeRegister;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(BAUD);
#ifdef USE_ARDUINO
  PIN11 = PIN(11);
  PIN12 = PIN(12);
  PIN13 = PIN(13);
  pinMode(PIN11, OUTPUT);
  digitalWrite(PIN11, LOW);
  pinMode(PIN12, OUTPUT);
  digitalWrite(PIN12, LOW);
  pinMode(PIN13, OUTPUT);
  digitalWrite(PIN13, LOW);
#else 
  OUTPUT_MASK =  (PIN(11) | PIN(12) | PIN(13));
  _pinModeRegister   = (uint8_t *) portModeRegister(OUTPUT_PORT);
  *_pinModeRegister |= OUTPUT_MASK;
  _outputRegister    = portOutputRegister(OUTPUT_PORT);
  *_outputRegister  &= ~OUTPUT_MASK;
#endif
  Serial.println("ready");
}

void loop() {
  // put your main code here, to run repeatedly:
  int stat = Serial.read();
  if (stat > 0) {
    uint8_t cmd = ((char)stat) << 3;
#ifdef USE_ARDUINO
    digitalWrite(PIN11, (cmd & PIN11)? HIGH:LOW);
    digitalWrite(PIN12, (cmd & PIN12)? HIGH:LOW);
    digitalWrite(PIN13, (cmd & PIN13)? HIGH:LOW);
#else
      *(_outputRegister) = cmd;
#endif
    Serial.print((char)stat);
  }
}

