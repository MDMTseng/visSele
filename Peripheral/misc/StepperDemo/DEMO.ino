//From:
// Generate stepper motor linear speed profile in real time
// M Y Stoychitch

//also good ref:
//Generate stepper-motor speed profiles in real time  Yr, 2004
//Applying acceleration and deceleration profiles to bipolar stepper motors
//AVR446: Linear speed control of stepper motor

#include <Arduino.h>

#define K 4096  // number of steps per one revolution
#define F 10000 // interrupt frequency 10kHz
//

// #define byte unsigned char
// #define TWO_PI (3.14159*2)
// #define boolean bool



float s = TWO_PI, vs = 0.5, vM = 2, aM = 2.0, m = 2, T, ta, tv, td, ss, aa = aM, vv = vM; // a[rad/sec^2],v[rad/sec],s[rad]
double alfa = TWO_PI / K, Af = aM / alfa / F / F, Vsf = vs / alfa / F, Df = -Af / m,      // Af[steps/Ts^2],Vsf[steps/Ts]
    brzina = Vsf, pozicija = 0;                                                           //
// cN-current number of steps, dN-desired number of steps
long N, Na, Nv, Nd, broj, cN, np = -1;
byte cw[] = {0b0001,  // A,
             0b0011,  // A,B
             0b0010,  // B,
             0b0110,  // B,C
             0b0100,  // C,
             0b1100,  // C,D
             0b1000,  // D,
             0b1001}; // D,A
byte dcw = sizeof(cw), kk = 0, maska = 0xF0, ledPin = 13;
char ch, chh;
boolean smjer = true, stoj = true;
//
void setup()
{
  Serial.begin(9600);
  Serial.print("CONTROL of STEPPER MOTOR\n");
  Serial.print("Commands: Pxxxx, desired position s[deg]X10 \n");
  Serial.print(" Bxxx, start speed vs [rad/sec]X10 \n");
  Serial.print(" Vxxx, max. speed vM [rad/sec]X10 \n");
  Serial.print(" Axxx, max. acce aM [rad/sec2]X10 \n");
  Serial.print(" Rxxx, the ratio m*10 of acce/dece \n");
  Serial.print(" S,-stop, M,- move, W-write paramet. \n");
  //
  DDRB = 0b00001111;       // pins 8,9,10,11 are outputs
  PORTB = PORTB & 0xF0;    // all outputs are zero
  pinMode(ledPin, OUTPUT); // led pin, this pin start and stop is labeled
  // settings interrupt, every 1/10[ms] the interrupt has occurred

  cli();      // disable global interrupts
  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1 = 0;  // initialize counter value to 0
  // dF-desired interrupt feq., pS- prescaler, cR-compare match register
  // cR=[16*10^6/(pS*dF)]-1
  // if dF=10^4, pS=1(no prescaler) => cR=[16*10^6/(1*10^4)]-1=1599
  // set compare match register to desired timer count:
  OCR1A = 1599;
  TCCR1B |= (1 << WGM12);  // turn on CTC mode:
  TCCR1B |= (1 << CS10);   // Set CS10 bit for no prescaler:
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt:
  sei();                   // enable global interrupts
}
// interrupt procedure, every 1/10000 [sec]=1/10[ms] program calls this procedure
ISR(TIMER1_COMPA_vect)
{
  //
  if (stoj)
    return;
  brzina = brzina + Af;
  pozicija = pozicija + brzina;
  if (pozicija - np >= 1)
  {
    oneStep(smjer);
    np++;
  }
}
// motor makes one step, if right = 1, then direction = CW
void oneStep(boolean right)
{
  if (right)
  {
    PORTB = (PORTB & 0xF0) | cw[kk++];
    cN++;
  }
  else
  {
    PORTB = (PORTB & 0xF0) | cw[dcw - 1 - kk++];
    cN--;
  }
  if (kk == dcw)
    kk = 0;
}
//
void loop()
{
  if (np >= Na && np < N - Nd)
    Af = 0; // speed is constant
  if (np >= N - Nd)
    Af = Df; // begin of deceleration
  if (np >= N)
  {
    stoj = true;
    Serial.print("..............................................\n");
    Serial.print("STEPS = ");
    Serial.print(cN);
    Serial.print(" [steps]\n");
    Serial.print("ANGLE = ");
    Serial.print(cN * 360.0 / K, 2);
    Serial.print(" [deg]\n");
    Serial.print("POSITION = ");
    Serial.print(float(cN * alfa), 2);
    Serial.print(" [rad]\n");
    np = -1;
    pozicija = 0;
    brzina = Vsf;
    digitalWrite(ledPin, LOW);
  }
}
// this procedure enables the entry of new data
void serialEvent()
{
  if (Serial.available() > 0)
  {
    ch = toupper(Serial.read());
    if (ch == 'P' || ch == 'B' || ch == 'V' || ch == 'A' || ch == 'W' || ch == 'R' || ch == 'S' || ch == 'M')
      chh = ch;
    if (ch >= '0' && ch <= '9')
      broj = broj * 10 + int(ch - '0');
    // analiza komandi
    if (ch == ',')
    {
      if (chh == 'P')
      {
        s = broj / 10.0; // s[deg]
        stoj = planTrajek(s);
      }
      if (chh == 'B')
        vs = broj / 10.0;
      if (chh == 'V')
        vM = broj / 10.0;
      if (chh == 'A')
        aM = broj / 10.0;
      if (chh == 'R')
        m = broj / 10.0;
      if (chh == 'S')
        stoj = true;
      if (chh == 'M')
        stoj = false;
      if (chh == 'W')
        pisiSve();
      broj = 0;
    }
  }
}
// this procedure displaying current data
void pisiSve()
{
  Serial.print("\n..............................................\n");
  Serial.print("Displacement s = ");
  Serial.print(s, 2);
  Serial.print("[deg]\n");
  Serial.print("Start. speed vs = ");
  Serial.print(vs, 2);
  Serial.print("[rad/sec]\n");
  Serial.print("Max. speed vM = ");
  Serial.print(vM, 2);
  Serial.print("[rad/sec]\n");
  Serial.print("Max. accler. aM = ");
  Serial.print(aM, 2);
  Serial.print("[rad/sec^2]\n");
  Serial.print("Ratio of a/d m = ");
  Serial.print(m, 2);
  Serial.print("\n");
  Serial.print("Stop = ");
  Serial.print((stoj) ? 1 : 0);
  Serial.print(", Move = ");
  Serial.print((stoj) ? 0 : 1);
  Serial.print("\n");
  Serial.print("..............................................\n");
}
// trajectory planning procedure
boolean planTrajek(float _s)
{
  Serial.print("\nDATA: newP = ");
  Serial.print(_s, 2);
  Serial.print("[deg], ");
  Serial.print(", curP = ");
  Serial.print(float(cN * 360.0 / K));
  Serial.print("[deg]");
  //
  N = int(_s * K / 360.0) - cN; // difference from desired and current position [steps]
  if (N < 0)
    smjer = false;
  else
    smjer = true;
  if (N == 0)
  {
    np = -1;
    return true;
  } //
  //
  N = abs(N);
  float s_ = N * alfa;
  T = sqrt(pow((vs * (1 + m) / aa), 2) + 2 * s_ * (1 + m) / aa) - vs * (1 + m) / aa; // N = round(s/alfa);
  ta = T / (1 + m);
  vv = vs + aa * ta; // td = (T-ta); tv = 0;
  if (vv <= vM)
  { // TRIANGULAR PROFILE
    Na = int(N / (1 + m));
    Nd = N - Na;
    Nv = 0;
  }
  else
  { // TRAPEZOIDAL PROFILE
    vv = vM;
    ta = (vM - vs) / aa;
    T = (1 + m) * ta;
    ss = vs * T + aa * pow(T, 2) / (2 * (1 + m));
    // ss - is tha part of movement in the phases of acceleration and deceleration only
    // np - number of steps on the movement ss
    np = int(ss / alfa);
    T = (1 + m) * ta + (s_ - ss) / vv; // tv = (s_-ss)/vv; td = T - ta - tv;
    Na = int(np / (m + 1));
    Nd = np - Na;
    Nv = N - np; // Nv - number of steps in the phase v=const.
  }
  // Vsf [steps/0.1ms] - the start speed, Af[steps/(0.1ms)^2]-acceleration, Df-deceleration
  Vsf = vs / alfa / F;
  Af = aa / alfa / F / F;
  Df = -Af / m;
  brzina = Vsf;
  pozicija = 0;
  np = 0;

  Serial.print(", dN = ");
  Serial.print(N);
  Serial.print(", Direction = ");
  Serial.print((smjer) ? "CW.\n" : "CCW.\n");
  digitalWrite(ledPin, HIGH);
  return false;
  //
}