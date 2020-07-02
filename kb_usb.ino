#include <Arduino.h>
#include <hidboot.h>
#include <usbhub.h>
#include <Keyboard.h>

// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>
#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>

#include "config.h"
#include "parser.h"
#include "report.h"

//#define DEBUG true

#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.print (x)
  #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

/*
 * USB Host Shield HID keyboards
 * This supports two cascaded hubs and four keyboards
 */
USB usb_host;
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    kbd0(&usb_host);
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    kbd1(&usb_host);
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    kbd2(&usb_host);
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    kbd3(&usb_host);
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    kbd4(&usb_host);
KBDReportParser kbd_parser0;
KBDReportParser kbd_parser1;
KBDReportParser kbd_parser2;
KBDReportParser kbd_parser3;
KBDReportParser kbd_parser4;
int usb_state;

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);


// Integrated key state of all keyboards
static report_keyboard_t keyboard_report;

static void or_report(report_keyboard_t report) {
    // integrate reports into keyboard_report
    keyboard_report.mods |= report.mods;
    for (uint8_t i = 0; i < KEYBOARD_REPORT_KEYS; i++) {
        if (IS_ANY(report.keys[i])) {
            for (uint8_t j = 0; j < KEYBOARD_REPORT_KEYS; j++) {
                if (! keyboard_report.keys[j]) {
                    keyboard_report.keys[j] = report.keys[i];
                    break;
                }
            }
        }
    }
}

void sendKey() {
  Keyboard.set_modifier(keyboard_report.mods);
  Keyboard.set_key1(keyboard_report.keys[0]);
  Keyboard.set_key2(keyboard_report.keys[1]);
  Keyboard.set_key3(keyboard_report.keys[2]);
  Keyboard.set_key4(keyboard_report.keys[3]);
  Keyboard.set_key5(keyboard_report.keys[4]);
  Keyboard.send_now();

  char cmdbuf[48];
  char fmtbuf[64];
  strcpy_P(fmtbuf, PSTR("AT+BLEKEYBOARDCODE=%02x-00-%02x-%02x-%02x-%02x-%02x"));
  snprintf(cmdbuf, sizeof(cmdbuf), fmtbuf, keyboard_report.mods, keyboard_report.keys[0], keyboard_report.keys[1], keyboard_report.keys[2], keyboard_report.keys[3], keyboard_report.keys[4]);
  DEBUG_PRINTLN(cmdbuf);
  ble.println(cmdbuf);
}

void initKb() {
  Keyboard.begin();
}

void initUsb() {
  if (usb_host.Init() == -1)
    DEBUG_PRINTLN("OSC did not start.");

  kbd0.SetReportParser(0, (HIDReportParser*)&kbd_parser0);
  kbd1.SetReportParser(0, (HIDReportParser*)&kbd_parser1);
  kbd2.SetReportParser(0, (HIDReportParser*)&kbd_parser2);
  kbd3.SetReportParser(0, (HIDReportParser*)&kbd_parser3);
  kbd4.SetReportParser(0, (HIDReportParser*)&kbd_parser4);
}

void initBle() {
  if (!ble.begin(false)) {
    DEBUG_PRINTLN("Couldn't find Bluefruit");
  }
  /* Disable command echo from Bluefruit */
  ble.echo(false);

  /* Change the device name to make it easier to find */
  ble.sendCommandCheckOK(F("AT+GAPDEVNAME=Realforce R2"));

  /* Enable HID Service */
  if (ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION)) {
    if (!ble.sendCommandCheckOK(F("AT+BleHIDEn=On"))) {
      DEBUG_PRINTLN("Could not enable Keyboard");
    }
  } else {
    if (!ble.sendCommandCheckOK(F("AT+BleKeyboardEn=On"))) {
      DEBUG_PRINTLN("Could not enable Keyboard");
    }
  }

  /* Add or remove service requires a reset */
  if (!ble.reset()) {
    DEBUG_PRINTLN("Couldn't reset??");
  }
}

void setup() {
  // put your setup code here, to run once:
#ifdef DEBUG
  Serial.begin( 115200 );
#if !defined(__MIPSEL__)
  while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
#endif
  initKb();
  initUsb();
  initBle();
}

void loop() {
  // put your main code here, to run repeatedly:
  static uint16_t last_time_stamp0 = 0;
  static uint16_t last_time_stamp1 = 0;
  static uint16_t last_time_stamp2 = 0;
  static uint16_t last_time_stamp3 = 0;
  static uint16_t last_time_stamp4 = 0;
  
  // check report came from keyboards
  if (kbd_parser0.time_stamp != last_time_stamp0 ||
    kbd_parser1.time_stamp != last_time_stamp1 ||
    kbd_parser2.time_stamp != last_time_stamp2 ||
    kbd_parser3.time_stamp != last_time_stamp3 ||
    kbd_parser4.time_stamp != last_time_stamp4) {

    DEBUG_PRINTLN("--------------------");

    last_time_stamp0 = kbd_parser0.time_stamp;
    last_time_stamp1 = kbd_parser1.time_stamp;
    last_time_stamp2 = kbd_parser2.time_stamp;
    last_time_stamp3 = kbd_parser3.time_stamp;
    last_time_stamp4 = kbd_parser4.time_stamp;

    // clear and integrate all reports
    keyboard_report = {};
    or_report(kbd_parser0.report);
    or_report(kbd_parser1.report);
    or_report(kbd_parser2.report);
    or_report(kbd_parser3.report);
    or_report(kbd_parser4.report);
    sendKey();
  }
  
  usb_host.Task();
  
  int new_state = usb_host.getUsbTaskState();
  if (usb_state != new_state) {
    usb_state = new_state;
    DEBUG_PRINT("usb_state: ");
    DEBUG_PRINTLN(usb_state);
    if (usb_state == USB_STATE_RUNNING) {
      DEBUG_PRINT("speed: ");
      DEBUG_PRINTLN(usb_host.getVbusState()==FSHOST ? "full" : "low");
    }
  }
}
