// clang-format off
// ============================================================
//  settings-custom.h — pin del nostro box (HAL 99)
// ------------------------------------------------------------
//  La copia buona sta in boxaudiobimbo/firmware/config/ e arriva nel fork
//  con ./firmware/applica-config.sh. Fonte dei numeri: hardware/pinout.md.
//  Scheda: ESP32-S3-DevKitC-1 N16R8.
//
//  Da evitare su questa scheda: 35-37 (PSRAM octal), 19-20 (USB nativa),
//  3/45/46 (strapping). GPIO0 e' strapping ma e' il tasto BOOT, e lo
//  usiamo apposta come tasto genitori.
// ============================================================

#ifndef __ESPUINO_SETTINGS_CUSTOM_H__
    #define __ESPUINO_SETTINGS_CUSTOM_H__
    #include "Arduino.h"

    // ---------------- microSD (SD_MMC 1 bit) ----------------
    // Sull'ESP32-S3 i pin di SD_MMC passano dalla matrice GPIO e il core
    // Arduino non ne assegna nessuno per la variante esp32s3: senza una
    // SD_MMC.setPins() la scheda non si monta e basta. Questi tre nomi
    // sono nostri e li legge SdCard.cpp (modifica marcata [BOX]).
    #define BOX_SD_CLK                      39
    #define BOX_SD_CMD                      38
    #define BOX_SD_D0                       40

    // ---------------- lettore NFC PN5180 (SPI dedicato) ----------------
    #define RST_PIN                         99          // dummy, richiesto da ESPuino
    #define RFID_CS                         11
    #define RFID_MOSI                        9
    #define RFID_MISO                       10
    #define RFID_SCK                         8

    #if defined(RFID_READER_TYPE_RUNTIME)
        #define RFID_BUSY                   41
        #define RFID_RST                    48          // verificare il LED RGB sulla DevKitC-1
        #define RFID_IRQ                     1          // deve essere un GPIO RTC (0-21): sveglia dal deep sleep
    #endif

    // ---------------- audio I2S ----------------
    #define I2S_DOUT                         6
    #define I2S_BCLK                         4
    #define I2S_LRC                          5

    // Abilitazione dell'amplificatore: basso = ampli spento (niente "toc")
    #define GPIO_PA_EN                      18

    // ---------------- tasti ----------------
    // 99 = disabilitato. Nessuna azione lunga, nessuna combinazione:
    // le azioni sono in settings-override.h.
    #define NEXT_BUTTON                     21          // avanti (GPIO RTC: sveglia il box)
    #define PREVIOUS_BUTTON                 47          // indietro
    #define PAUSEPLAY_BUTTON                 0          // tasto genitori sul fondo (in parallelo a BOOT)
    #define ROTARYENCODER_BUTTON            99
    #define BUTTON_4                        99
    #define BUTTON_5                        99

    // Risveglio dal deep sleep: il tasto avanti (GPIO RTC).
    // L'altro risveglio e' l'appoggio della statuina, via RFID_IRQ (LPCD).
    #define WAKEUP_BUTTON                   NEXT_BUTTON

    // ---------------- I2C ----------------
    // ESPuino accende sempre il secondo bus I2C quando c'e' la selezione del
    // lettore a runtime (gli serve per l'MFRC522 via I2C), quindi questi due
    // nomi devono esistere. Ci attacchiamo il DS3231.
    // Sono i pin di UART0: i log escono dalla USB nativa.
    #ifdef I2C_2_ENABLE
        #define ext_IIC_DATA                43         // SDA
        #define ext_IIC_CLK                 44         // SCL
    #endif

    // ---------------- misura della batteria ----------------
    #ifdef MEASURE_BATTERY_VOLTAGE
        #define VOLTAGE_READ_PIN             7
        constexpr float s_offsetVoltage = 0.00;         // taratura vera dalla web UI, nel task 1
        constexpr uint16_t rdiv1 = 100;                 // kOhm verso VBAT
        constexpr uint16_t rdiv2 = 100;                 // kOhm verso massa
        constexpr adc_attenuation_t inputAttenuation = ADC_11db;
    #endif

    // ---------------- nostri, non di ESPuino ----------------
    // Usati dai moduli nuovi (task 1, 2): non sono nomi che ESPuino conosce.
    #define BOX_CURSORE_ADC                  2          // cursore volume (ADC1)
    #define BOX_CURSORE_PWR                 42          // alimenta il cursore solo da svegli
    // GPIO 12-17 liberi: erano del display e-paper, tolto il 18 ottobre.
#endif
