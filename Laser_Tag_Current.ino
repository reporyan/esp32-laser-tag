//delete piezooff code and just add notone to timer ????

// C++ code
#define DECODE_NEC 1

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <Arduino.h>

//OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//pins
const int hitLED = 16;
const int fireLED = 32;
const int piezo = 2; // was on 2
const int trigger = 17;
const int reload = 13;
const int irReceiver = 18;
const int muzzleFlashLED = 19;

//could add classes with abilities like shield, changing reload etc

//objects
struct Gun
{
  const char* name;//gun display name. limited characters due to display
  int damage;//damage
  int maxAmmo;//capacity, reloads to this amount in magazine
  bool isAutomatic;//whether you can continuously fire by holding trigger
  int chamberTime;//time between shots in ms
  int reloadTime;//time it takes to fully reload mag in mc
  unsigned long irCode;//infared code sent and received
  int gunType;//0 = gun, 1 = remote trap (fires when triggered)
};

//gun array. if you have trouble with codes, perhaps make codes ordered by damage. We don't go bellow 100ms chamber time. reload runs concurrently with chamber.
Gun guns[] = 
{
  //standard guns
  {"Pistol", 20, 16, false, 250, 1250, 0x01010101, 0},
  {"Rifle", 16, 30, true, 200, 2750, 0x02020202, 0},
  {"Sniper", 90, 5, false, 2000, 3750, 0x03030303, 0},
  {"SMG", 16, 20, true, 150, 2250, 0x04040404, 0},
  {"PresRifle", 55, 12, false, 800, 3500, 0x05050505, 0},
  {"HevSniper", 110, 1, false, 3000, 5000, 0x06060606, 0},
  {"Revolver", 45, 6, false, 550, 3250, 0x07070707, 0},
  {"Minigun", 8, 60, true, 100, 6250, 0x08080808, 0},
  {"MacPistol", 15, 10, true, 100, 2250, 0x09090909, 0},
  //drum gun, silenced pistol, guns with 2 firing modes instead of reload, status effects, overheating gun, burst fire gun

  //unique guns
  {"ToyGun", 10, 12, false, 400, 2500, 0x0A0A0A0A, 0},
  {"Healer", -5, 10, true, 250, 2750, 0x0B0B0B0B, 0},

  //traps
  {"RmoteTrap", 40, 10, true, 100, 5000, 0x0C0C0C0C, 1},

  //testing guns only, should not be in a release
  {"TestGun", 0, 999, true, 100, 200, 0x0D0D0D0D, 0},
  {"GodGun", 999, 999, true, 100, 200, 0x0E0E0E0E, 0}
};

struct Input
{
  bool pTriggerIn = false;
  bool pReloadIn = false;

  bool triggerIn = false;
  bool reloadIn = false;

  bool semiTriggerIn = false;
  bool semiReloadIn = false;

  void GetInput()
  {
    triggerIn = digitalRead(trigger);
    reloadIn = digitalRead(reload);

    semiTriggerIn = triggerIn && !pTriggerIn;
    semiReloadIn = reloadIn && !pReloadIn;
  }

  void PreviousInput()
  {
    pTriggerIn = digitalRead(trigger);
    pReloadIn = digitalRead(reload);
  }
};

struct Timer
{
  bool isTiming;
  unsigned long endTime;
  unsigned long startTime;

  void Start(unsigned long _time)
  {
    isTiming = true;
    startTime = millis();
    endTime = millis() + _time;
  }

  void Stop()
  {
    isTiming = false;
  }

  bool Done()
  {
    //if timing and done
    if(isTiming && millis() >= endTime)
    {
      isTiming = false;
      return true;
    }
    
    return false;
  }

  float GetDecimalProgress()
  {
    if (!isTiming) return 0;

    return constrain(((millis() - startTime) / (float)(endTime - startTime)), 0.0, 1.0);
  }
};

//timers
Timer piezoTimer;
Timer immuneTimer;
Timer chamberTimer;
Timer reloadTimer;

//input handler
Input input;

int gunAmount = sizeof(guns) / sizeof(guns[0]);

//player data
int gunID = 0;
Gun gun;

int maxHealth = 100;
int health = maxHealth;

int ammo = 0;
int deaths = 0;

const float immuneTime = 68; //works on 68. the ir stays on for 2 frames i think. having it exact on the time the ir light is on is nesecary since if someone shoots at the same time as an enemy, their immune period will finish before the enemy's shot (given you shoot first). You become immune when shooting or when shot
float deathTime = 3000; //time between death and respawn

bool piezoOn = false;
bool muzzleFlashLEDOn = false;

bool immune = false;
bool reloading = false;
bool chambered = false; //false to remove glitch where spawning shoots??

bool active = false; //change later

bool trapArmed = false;
bool trapFire = false;

//int hitCode = 0;
//int shootCode = 0;
//int gunAmount = *(&damages + 1) - damages;
//extra
int playing;
int piezoFreq = 0;

//ir initialising
IRrecv irrecv(irReceiver);
IRsend irsend(fireLED);
decode_results results;
unsigned long irReceiveCode = 0x0;
unsigned long irSendCode = 0x0;

//setup
void setup()
{
  //serial
  Serial.begin(115200);
  Serial.println("=== Laser Gun Starting... ===");

  //pins
  pinMode(hitLED, OUTPUT);
  pinMode(muzzleFlashLED, OUTPUT);
  pinMode(piezo, OUTPUT);

  pinMode(trigger, INPUT);
  pinMode(reload, INPUT);
  pinMode(irReceiver, INPUT);

  //ir
  irrecv.enableIRIn();
  irsend.begin();
  
  ledcAttach(piezo, 2000, 8);//new

  //OLED
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.display();
  display.clearDisplay();
  SwitchGun(0);

  //Hardware Test
  digitalWrite(hitLED, HIGH);
  digitalWrite(fireLED, HIGH);
  digitalWrite(muzzleFlashLED, HIGH);
  tone(piezo, 700);

  delay(500);

  digitalWrite(hitLED, LOW);
  digitalWrite(fireLED, LOW);
  digitalWrite(muzzleFlashLED, LOW);
  NoTone();

  //success
  Serial.println("=== Laser Gun Startup Successful! ===");
}

//
//LOOP
//

void loop()
{
  //debug
  //PrintDebug();

  //get input
  input.GetInput();

  //basic looping
  ReceiveIR();
  HandleTrapFire();
  PiezoOff();//??????^^^
  
  //timers
  UpdateTimers();

  //in menu
  if(!active)//change this to a state int
  {
    HandleSelect();
    HandleSwitchGun();
  }
  //in game
  else
  {
    HandleShoot();
    HandleDisarmTrap();
    HandleReload();//implement quick reload!
  }

  //turn light off if it was on
  MuzzleFlashOff();

  //get input to compare for one time use buttons
  input.PreviousInput();

  //we were gonna have to do this eventually...
  UpdateOled();

  //necessary delay, otherwise IR cannot receive properly
  delay(20);
}

//
//METHODS
//

void PrintDebug()
{
  Serial.println("--- Debug Start ---");
  Serial.print("Immune: ");
  Serial.println(immune);
}

void UpdateTimers()
{
  //piezo
  if(piezoTimer.Done())
  {
    piezoOn = false;
    //just use NoTone here?????
  }

  //immune
  if(immuneTimer.Done())
  {
    immune = false;
    digitalWrite(hitLED, LOW);
    display.invertDisplay(false);
  }

  //chamber
  if(chamberTimer.Done())
  {
    chambered = true;   
  }

  //reload
  if(reloadTimer.Done())
  {
    reloading = false;
    ammo = gun.maxAmmo;
  }
}

void MuzzleFlashOff()
{
  //Muzzle Flash
  if(muzzleFlashLEDOn)
  {
    digitalWrite(muzzleFlashLED, LOW);
    muzzleFlashLEDOn = false;
  }
}

void StartGame()
{
  Serial.println("--- Game Active ---");

  //start game
  active = true; //pre start
  immune = false;
  chambered = false;
  reloading = false;
  ammo = gun.maxAmmo;

  chamberTimer.Start(gun.chamberTime);
}

void HandleSwitchGun()
{
  if(input.semiReloadIn)
  {
    NextGun();
    ActivatePiezo(piezo, 200, 100); //piezo sound
  }
}

void HandleReload()
{
  if(input.semiReloadIn && !reloading && ammo < gun.maxAmmo)
  {
    reloadTimer.Start(gun.reloadTime);
    ActivatePiezo(piezo, 200, gun.reloadTime);
    reloading = true;
  }
}

void HandleSelect()
{
  if(input.semiTriggerIn)
  {
    StartGame(); 
  }
}

void HandleDisarmTrap()
{
  if(input.triggerIn && gun.gunType == 1)//disabling trap
  {
    reloading = false;
    trapArmed = false;
    ammo = 0;
  }  
}

void HandleShoot()
{
  if(((input.triggerIn && gun.isAutomatic) || input.semiTriggerIn) && ammo > 0 && !reloading && chambered && gun.gunType == 0)
  {
    Shoot();
  }
}

void PiezoOff()
{
  //Piezo
  //investigate this, can use millis to determine if NoTone should be used on this frame?
  if(!piezoOn)
  {
    NoTone(); //NoTone dowsnt work after new computer :/, so i had to use dwrite \/
    //digitalWrite(piezo, LOW);
  }
}

void HandleTrapFire()
{
  //trap fire
  if(trapFire && ammo >= 1 && !reloading && chambered && gun.gunType == 1)
  {
    Shoot();

    if(ammo <= 0)
    {
      trapFire = false;
    }
  }
}

void ReceiveIR()
{
  irReceiveCode = 0;

  if(irrecv.decode(&results))
  {
    irReceiveCode = results.value;

    Serial.print("> IR Signal Received: ");
    Serial.println(results.value, HEX);

    irrecv.resume();

    //process as gun
    if(active && !immune)
    {
      for (int i = 0; i < gunAmount; i++)
      {
        if(irReceiveCode == guns[i].irCode)
        {
          //should immunity and LED go in a seperate function? (maybe doesn't have to be take damage function)

          //immune timer
          immuneTimer.Start(immuneTime);
          immune = true;
          display.invertDisplay(true);

          //hit LED
          digitalWrite(hitLED, HIGH);

          //hit Sound
          ActivatePiezo(piezo, 700, immuneTime);

          //gun
          if(gun.gunType == 0)
          {
            TakeDamage(guns[i].damage);
          }
          //trap
          else if(gun.gunType == 1 && !trapFire)
          {
            //shoot
            trapFire = true;
          }
        }
      }
    } 
  }
}

void TakeDamage(int _damage)
{
  //change hp
  health -= _damage;

  if(health <= 0)
  {
    Death();
  }
  else if(health > 100)
  {
    health = maxHealth;
  }
}

void Shoot()
{
  Serial.print("< Sending IR: ");
  Serial.println(gun.irCode, HEX);

  chamberTimer.Start(gun.chamberTime);
  chambered = false;

  //change ammo
  ammo -= 1;

  //go immune for a short period
  immuneTimer.Start(immuneTime);
  immune = true;

  //muzzle flash
  digitalWrite(muzzleFlashLED, HIGH); //turns off next frame
  muzzleFlashLEDOn = true;

  //PWM prioritises only last one for some reason

  //piezo
  ActivatePiezo(piezo, 500, 0);

  //shoot IR signal!
  irsend.sendNEC(gun.irCode, 32); //Send IR
}

void SwitchGun(int _gunID) //9 charcter name cap
{
  gunID = _gunID;
  gun = guns[gunID];
  
  Serial.print("Switched Gun To: ");
  Serial.println(gun.name);
}

void NextGun()
{
  gunID++;
  if(gunID >= gunAmount)
  {
    gunID = 0;
  }

  SwitchGun(gunID);
}

void Death()
{
  digitalWrite(hitLED, HIGH);
  ActivatePiezo(piezo, 900, deathTime);//I guess this works...
  
  health = 0;
  deaths++;

  //we can update screen here because it goes into a delay
  UpdateOled();

  delay(deathTime);
  
  digitalWrite(hitLED, LOW);

  health = 100;
  chambered = true;
  reloading = false;
  ammo = gun.maxAmmo;
  
  //immuneTimer.Start(deathTime);
  //immune = true;
  active = false;
  //piezoOn = false;

  SwitchGun(gunID);
}

const unsigned char heartBig [] PROGMEM =
{
  0b00110011, 0b00000000,
  0b01111111, 0b10000000,
  0b11111111, 0b11000000,
  0b11111111, 0b11000000,
  0b11111111, 0b11000000,
  0b01111111, 0b10000000,
  0b00111111, 0b00000000,
  0b00011110, 0b00000000,
  0b00001100, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000
};

const unsigned char ammoBig [] PROGMEM =
{
  0b00001100, 0b00000000,
  0b00001100, 0b00000000,
  0b00011110, 0b00000000,
  0b00011110, 0b00000000,
  0b00011110, 0b00000000,
  0b00000000, 0b000000000,
  0b00011110, 0b00000000,
  0b00011110, 0b00000000,
  0b00011110, 0b00000000,
  0b00011110, 0b00000000,
  0b00011110, 0b00000000,
  0b00011110, 0b00000000
};

//I think we need a death display

void DisplayMenu()
{
  if(gun.gunType == 0) //gun
  {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(gunID);
    display.setTextSize(2);
    display.print(gun.name);
    display.println();
    display.print("DMG: ");
    display.println(gun.damage);
    display.print("CAP: ");
    display.println(gun.maxAmmo);
    display.print("RLD: ");
    display.println((float)gun.reloadTime / 1000); 
    display.display();
  }
  else if(gun.gunType == 1)
  {
    ammo = 0; //needs to be this way, otherwise gun will not be able to arm because it cant reload.

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(gunID);
    display.setTextSize(2);
    display.print(gun.name);
    display.println();
    display.print("DMG: ");
    display.println(gun.damage);
    display.print("CAP: ");
    display.println(gun.maxAmmo);
    display.print("ARM: ");
    display.println((float)gun.reloadTime / 1000); 
    display.display();
  }
}

void DisplayActive()
{
  if(gun.gunType == 0)
  {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);

    //health text
    display.drawBitmap(30, 18, heartBig, 12, 12, SSD1306_WHITE);
    display.setCursor(42, 16);
    display.print(health);

    //ammo text
    display.drawBitmap(30, 34, ammoBig, 12, 12, SSD1306_WHITE);
    display.setCursor(42, 34);
    display.print(ammo);

    //correct displacement
    display.setTextSize(1);
    if(ammo >= 10)
      display.setCursor(66, 40);
    else if(ammo >= 100)
      display.setCursor(78, 40);
    else
      display.setCursor(54, 40);

    display.print("/");
    display.print(gun.maxAmmo);
    
    //health bar
    display.drawRect(4, 4, 16, 56, SSD1306_WHITE);
    display.drawRect(5, 5, 14, 54, SSD1306_WHITE);

    int healthPixels = ((float)health / maxHealth) * 48;
    display.fillRect(8, 56 - healthPixels, 8, healthPixels, SSD1306_WHITE);

    //ammo bar
    display.drawRect(108, 4, 16, 56, SSD1306_WHITE);
    display.drawRect(109, 5, 14, 54, SSD1306_WHITE);

    if(!reloading)//reloading handled below, since multiple gun types do it ig. //should change OLED functions for more versatility later, shouldn't have different code
    {
      int ammoPixels = ((float)ammo / gun.maxAmmo) * 48;
      display.fillRect(112, 56 - ammoPixels, 8, ammoPixels, SSD1306_WHITE);
    }
  }
  //trap
  else if(gun.gunType == 1)
  {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    if(ammo == 0)
      display.println("DISABLED");
    else
      display.println("ARMED");
    display.print("AM: ");
    display.println(ammo);
    display.setTextSize(1);
    display.print(gunID);
    display.setTextSize(2);
    display.println(gun.name);
  }

  //reload bar
  if(reloading)
  {
    int reloadPixels = reloadTimer.GetDecimalProgress() * 48;
    display.fillRect(112, 56 - reloadPixels, 8, reloadPixels, SSD1306_WHITE);
  }
}

void UpdateOled()
{
  display.clearDisplay();

  if(!active)
  {
    DisplayMenu();
  }
  else
  {
    //gun
    DisplayActive();
  }

  display.display();
}

void ActivatePiezo(int _pin, int _freq, int _onTime)
{
  piezoTimer.Start(_onTime);
  
  piezoOn = true;
  tone(_pin, _freq);
}

void tone(byte _pin, int _freq)
{
  //ledcSetup(2, 2000, 8); // setup beeper. change back to 1 if glitch
  ledcWriteTone(_pin, _freq); // play tone
  playing = _pin; // store pin
}

void NoTone()
{
  tone(piezo, 0);
  digitalWrite(piezo, LOW);
}