// libs
#include <Wire.h> 
#include <string.h>
#include <LiquidCrystal_I2C.h>
#include <TimerOne.h>
#include <RTClib.h>

#define printByte(args)  write(args);
#define TEMPERATURE_PRECISION 9

// display lcd 20x4
LiquidCrystal_I2C lcd(0x27, 20, 4);
// rtc for the DS3231 RTC module
RTC_DS3231 rtc;
// time variables
unsigned long timeout, tab_timeout, uptime_timer, plot_timer;
// string converter and perc
String string_convert;
String perc;
// default speed and temperature limit settings (in case of no connection)
byte speedMIN = 10, speedMAX = 90, tempMIN = 30, tempMAX = 70;

// degree icon
byte degree[8] = {0b11100,  0b10100,  0b11100,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000};
// right edge of loading bar
byte right_empty[8] = {0b11111,  0b00001,  0b00001,  0b00001,  0b00001,  0b00001,  0b00001,  0b11111};
// left edge of loading bar
byte left_empty[8] = {0b11111,  0b10000,  0b10000,  0b10000,  0b10000,  0b10000,  0b10000,  0b11111};
// center edge of loading bar
byte center_empty[8] = {0b11111, 0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111};
// blocks for plotting graphs
byte row8[8] = {0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
byte row7[8] = {0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
byte row6[8] = {0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
byte row5[8] = {0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
byte row4[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111};
byte row3[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111};
byte row2[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111};
byte row1[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111};

// array of input values (char)
char inData[82]; 
// array of numerical values of readings from a computer
int PCdata[20]; 
// array for storing data for plotting a graph (16 values for 6 parameters)
byte PLOTmem[6][16];

byte index = 0;
// default display mode
int display_mode = 0;
// some updating, timeout and restore booleans
boolean reDraw_flag = 1, updateDisplay_flag, restoreConnectToPC = 0, timeOutLCDClear = 0, rtc_editing = 0;
byte lines[] = {4, 5, 7, 6};
// 0-CPU temp, 1-GPU temp, 2-CPU load, 3-GPU load, 4-RAM load, 5-GPU memory
byte plotLines[] = {0, 1, 4, 5, 6, 7};

// Titles for chart legends
const char plot_0[] = "CPU";
const char plot_1[] = "GPU";
const char plot_2[] = "RAM";
const char plot_3[] = "temp";
const char plot_4[] = "load";
const char plot_5[] = "mem";
// The names below must match the arrays above and be in order
static const char *plotNames0[]  = {
  plot_0, plot_1, plot_0, plot_1, plot_2, plot_1
};
static const char *plotNames1[]  = {
  plot_3, plot_3, plot_4, plot_4, plot_4, plot_5
};

/*
  create array to convert digit days to words:
  0 = Sunday    |   4 = Thursday
  1 = Monday    |   5 = Friday
  2 = Tuesday   |   6 = Saturday
  3 = Wednesday |
*/
const char dayInWords[7][4] = {
  "sun", "mon", "tue", "wed", "thu", "fri", "sat"
};

/*
  create array to convert digit months to words:
  0 = [no use]  |
  1 = January   |   6 = June
  2 = February  |   7 = July
  3 = March     |   8 = August
  4 = April     |   9 = September
  5 = May       |   10 = October
  6 = June      |   11 = November
  7 = July      |   12 = December
*/
const char monthInWords[13][4] = {
  " ", "jan", "feb", "mar", "apr", "may", "jun", 
  "jul", "aug", "sep", "oct", "nov", "dec"
};

void setup() {
  Serial.begin(9600);

  // initialize rtc
  rtc.begin();

  // display initialize
  lcd.init();
  lcd.backlight();
  lcd.clear();
  show_startup();

  // delay for 3s
  delay(3000);               
  lcd.clear();

  PCdata[8] = speedMAX;
  PCdata[9] = speedMIN;
  PCdata[10] = tempMAX;
  PCdata[11] = tempMIN;
}

// loop
void loop() {
  /*
    for edit rtc, you need to uncomment this block of code,
    them type in serial port 'u' and enter real time  
  */
  // if (Serial.available()) 
  // {
  //   char input = Serial.read();
  //   if (input == 'u') 
  //   {
  //     rtc_editing = 1;
  //     updateRTC();
  //   }
  // }

  if (!rtc_editing)
  {
    parsing(); // parsing data from pc
    updatePlot(); // updating array data to graph
    updateDisplay(); // updating display
    timeoutTick(); // time tick
  }
}

// parsing data from open hardware monitor
void parsing() 
{
  while (Serial.available() > 0)
  {
    char aChar = Serial.read();

    if (aChar != 'E') 
    {
      inData[index] = aChar;
      index++;
      inData[index] = '\0';
    }
    else
    {
      char *p = inData;
      char *str;
      index = 0;
      String value = "";

      while ((str = strtok_r(p, ";", &p)) != NULL) 
      {
        string_convert = str;
        PCdata[index] = string_convert.toInt();
        index++;
      }

      index = 0;
      updateDisplay_flag = 1;
    }

    timeout = millis();
    restoreConnectToPC = 1;
  }
}
  
// update plot
void updatePlot() 
{
  if ((millis() - plot_timer) > (PCdata[17] * 1000)) 
  {
    // every column data
    for (int i = 0; i < 6; i++) 
    {

      // every row data (except last)
      for (int j = 0; j < 15; j++) 
      {
        // move all array to left
        PLOTmem[i][j] = PLOTmem[i][j + 1];
      }
    }

    for (int i = 0; i < 6; i++) 
    {
      // remember count of graphic bar to last item in array
      PLOTmem[i][15] = ceil(PCdata[plotLines[i]] / 3);
    }

    plot_timer = millis();
  }
}

// updating display
void updateDisplay() 
{
  if (updateDisplay_flag) 
  {
    if (reDraw_flag) 
    {
      lcd.clear();
      switch (display_mode) 
      {
        case 0:
          break;
        case 2: draw_pc_stats_label();
          break;
      }

      reDraw_flag = 0;
    }

    switch (display_mode) 
    {
      case 0: draw_time_and_date();
        break;
      case 2: draw_pc_stats();
        break;
    }

    updateDisplay_flag = 0;
  }

  if(timeOutLCDClear) reDraw_flag = 1;
}

// draw label for pc information
void draw_pc_stats_label() 
{
  lcd.createChar(0, degree);
  lcd.createChar(1, left_empty);
  lcd.createChar(2, center_empty);
  lcd.createChar(3, right_empty);
  lcd.createChar(4, row8);

  lcd.setCursor(0, 0);
  lcd.print("cpu:");
  lcd.setCursor(0, 1);
  lcd.print("gpu:");
  lcd.setCursor(0, 2);
  lcd.print("gpumem:");
  lcd.setCursor(0, 3);
  lcd.print("ramuse:");
}

// draw pc information
void draw_pc_stats() 
{
  timeOutLCDClear = 0;

  lcd.setCursor(4, 0); 
  lcd.print(PCdata[0]); 
  lcd.write(223);
  lcd.setCursor(17, 0); 
  lcd.print(PCdata[4]);
  if (PCdata[4] < 10) perc = "% ";
  else if (PCdata[4] < 100) perc = "%";
  else perc = "";  lcd.print(perc);

  lcd.setCursor(4, 1);
  lcd.print(PCdata[1]);
  lcd.write(223);
  lcd.setCursor(17, 1);
  lcd.print(PCdata[5]);
  if (PCdata[5] < 10) perc = "% ";
  else if (PCdata[5] < 100) perc = "%";
  else perc = "";  lcd.print(perc);

  lcd.setCursor(17, 2);
  lcd.print(PCdata[7]);
  if (PCdata[7] < 10) perc = "% ";
  else if (PCdata[7] < 100) perc = "%";
  else perc = "";  lcd.print(perc);

  lcd.setCursor(17, 3);
  lcd.print(PCdata[6]);
  if (PCdata[6] < 10) perc = "% ";
  else if (PCdata[6] < 100) perc = "%";
  else perc = "";  lcd.print(perc);

  for (int i = 0; i < 4; i++) 
  {
    byte line = ceil(PCdata[lines[i]] / 10);
    lcd.setCursor(7, i);
    
    if (line == 0) lcd.printByte(1)
    else lcd.printByte(4);

    for (int n = 1; n < 9; n++) 
    {
      if (n < line) lcd.printByte(4)
      else lcd.printByte(2);
    }

    if (line == 10) lcd.printByte(4)
    else lcd.printByte(3);
  }

}

// draw time and date information
void draw_time_and_date()
{
  updateDisplay_flag = 0;

  // get time and date from RTC and save in variables
  DateTime rtcTime = rtc.now();

  int ss = rtcTime.second();
  int mm = rtcTime.minute();
  int hh = rtcTime.twelveHour();
  int DD = rtcTime.dayOfTheWeek();
  int dd = rtcTime.day();
  int MM = rtcTime.month();
  int yyyy = rtcTime.year();

  // print date in dd-MMM-yyyy format and day of week
  lcd.setCursor(2, 0);
  if (dd < 10) lcd.print("0");  // add preceeding '0' if number is less than 10
  lcd.print(dd);
  lcd.print("-");
  lcd.print(monthInWords[MM]);
  lcd.print("-");
  lcd.print(yyyy);

  lcd.print(" ");
  lcd.print(dayInWords[DD]);

  // move LCD cursor to lower-left position
  lcd.setCursor(4, 1);

  // print time in 12H format
  if (hh < 10) lcd.print("0");
  lcd.print(hh);
  lcd.print(':');

  if (mm < 10) lcd.print("0");
  lcd.print(mm);
  lcd.print(':');

  if (ss < 10) lcd.print("0");
  lcd.print(ss);

  // print AM/PM indication
  if (rtcTime.isPM()) lcd.print(" pm");
  else lcd.print(" am");

  lcd.setCursor(5, 2);
  lcd.print("connection");

  // if no connection with pc
  lcd.setCursor(4, 3);
  if (!restoreConnectToPC) lcd.print("   failed   ");
  else lcd.print("established");
}

// function to update rtc time using user input
void updateRTC()
{
  lcd.clear();  // clear LCD display
  lcd.setCursor(0, 0);
  lcd.print("Edit Mode...");

  // ask user to enter new date and time
  const char txt[6][15] = 
  {
    "year [4-digit]", "month [1~12]", "day [1~31]",
    "hours [0~23]", "minutes [0~59]", "seconds [0~59]"
  };
  String str = "";
  long newDate[6];

  while (Serial.available()) 
  {
    Serial.read();  // clear serial buffer
  }

  for (int i = 0; i < 6; i++) 
  {
    Serial.print("Enter ");
    Serial.print(txt[i]);
    Serial.print(": ");

    while (!Serial.available()) {
      ; // wait for user input
    }

    str = Serial.readString();  // read user input
    newDate[i] = str.toInt();   // convert user input to number and save to array

    Serial.println(newDate[i]); // show user input
  }

  // update RTC
  rtc.adjust(DateTime(newDate[0], newDate[1], newDate[2], newDate[3], newDate[4], newDate[5]));
  Serial.println("RTC Updated!");
  // delay and updating display
  display_mode = 0;
  updateDisplay_flag = 1;
  reDraw_flag = 1;
  delay(3000);
  rtc_editing = 0;
}

// show logotype
void show_startup() 
{
  lcd.setCursor(4, 0);
  lcd.print("where is");
  lcd.setCursor(6, 1);
  lcd.print("your love?");
  lcd.setCursor(5, 3);
  lcd.print("just-sain");
}

// time out tick
void timeoutTick() 
{
  // if no connection with pc
  if (Serial.available() < 1)
  {
    if (millis() - timeout > 5000) 
    {   
      index = 0;

      if (restoreConnectToPC) restoreConnectToPC = 0;

      // switch to time and date
      if (display_mode != 0) 
      {
        display_mode = 0;
        reDraw_flag = 1;
      }

      updateDisplay_flag = 1;
    }
  }
  else if (millis() - tab_timeout > 10000) 
  {
    // every 10s tab change
    if (display_mode == 0) display_mode = 2;
    else display_mode = 0;

    updateDisplay_flag = 1;
    reDraw_flag = 1;

    tab_timeout = millis();
  }
}