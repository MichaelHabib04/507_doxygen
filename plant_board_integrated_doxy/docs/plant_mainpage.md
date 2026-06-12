![Caption Text](Splash.png)
**Introduction**
Our project for ME507 is a deep water culture hydroponics rig intended to autonomously raise herb plants. A bin contains water which the plant (in this case, a mint plant) lives in. Ideally, the roots of the plant are not totally submerged but do reach into the water. Nutrient levels and water levels are measured in the tank to ensure the plant remains in contact with the water and has sufficient nutrient solution for growth. Additionally, since the water remains stagnant, it needs to be oxygenated to supply the roots with oxygen. This is done using an air pump, which forces water through an air stone submerged in the bin.

A controller was also created to mesh with this system. The intention is to be able to switch between manual and autonomous mode for the rig. In autonomous mode, the actuators automatically operate when relevant sensor data is received, while in manual mode, buttons on the controller must be pressed to operate an actuator. LEDs display the most recent command from the buttons, and the LCD screen on the controller displays a fun character.

**Hardware**

**Mechanical Details**

3D printing was used for the bulk of the manufacturing, but the water reservoir was a purchased tub to ensure water would not leak out. Additionally, a wooden veneer was created from ¼” plywood which we stained to “Golden Oak.” 3D printed mounting stands were made to attach the wooden panels to each other, and JB Weld was used for epoxying. The water reservoirs were printed in PETG for water-resistant properties. Finally, a 3D printed sensor mount lays over the reservoir with holes for mounting the sensors and aeration stone and all tubing. For the plant itself, we printed a plant holder to hold a netted pot. Since Home Depot did not stock actual netted pots, we used a shower drain filled with rocks as a growing medium.

![Caption Text](plant_cad.png "3D model developed in Fusion 360")

![Caption Text](assembled.png "Assembled rig based on CAD")


The board was designed to take M3 screws, and the enclosure contains several 3D printed standoffs with heatset inserts to mount the board. Additionally, the enclosure mounts to the wooden veneer with M5 nuts and bolts. All wire lengths given with any sensors and actuators were found to be sufficiently sized, so a makeshift harness was created by zip tying cables together and feeding it into a hole in the enclosure.

Due to the mounting holes on the controller board being unusable, a controller housing was designed with slots to fit the board into.

![Caption Text](controller_layout.png "Controller Board Layout")

![Caption Text](controller.png "Controller as depicted in Fusion")

![Caption Text](controller_pg.jpg "Controller in real life")

**Plant Board**

The board was designed with screw terminals intended for attaching peripherals to the board. The board was ultimately 5” by 2.5” with power supplied through a barrel jack connector. The 12V volatage supplied powers two valves, and after a buck converter, the 5V line supports a motor and conductivity (TDS) probe. With an LDO, the voltage is further stepped down to 3.3V to power the MCU and float switch. The board features various test points to ensure proper voltages and signals are being sent, and for a bulk of the project, we powered the board through the 12V and GND test poins with a DC power supply with current limiting. SWD interface is done with a 6 pin header to interface with the ST Link, and we additionally broke out four spare GPIO pins and a spare UART. With the realization that the intended microB connector on the board was not operational without a crystal, we used the spare UART for all debugging through Putty.

![Caption Text](plant_board_layout.png "Plant Board Layout")

![Caption Text](3d_plant_board.png "3D model as developed in Fusion")

![Caption Text](real_p_board.jpg "Real board post assembly")

The motor driver issues PWM commands to set the motor to operate at 50% duty cycle as recommended by the Adafruit Pump manufacturers.

The valves are controlled with a PhotoMos relay from Panasonic. This relay uses LEDs and a mosfet, so when GPIO commands come from the MCU, the LED switches on, which closes the switch and allows the valve to open. The valves are normally closed for safety purposes, so they only open when commanded by the relay. One side of the relay takes MCU logic, while the other side takes 12V power and they are isolated by an airgap within the chip itself.

The TDS probe was intended for Arduino usage, so it came with a breakout board for handling analog signals. To ensure it would receive a less noisy into of 5V, we used a ferrite bean and decoupling capacitors to isolate the analog circuitry from the noisy buck converter output. Additionally, all analog circuitry was isolated from the rest of the circuit to ensure easier debugging. Finally, a connector to use the given breakout board was added to the main board to allow the use of the breakout board if the circuit did not work.

**Software**

**Controls – Plant Board**

![Caption Text](p_fsm.png "Plant System FSM/STD")

C was used for all the programming of the main board control loop because Malaika does not understand objects but she likes structs. An FSM was considered for this application, but due to the simplicity of the control scheme, it was deemed unnecessary. Control is possible with several if/else statements

On the main board, Valve1 is controlled by the float sensor and Valve2 is controlled by the TDS probe. Valve drivers were created through HAL_GPIO_WritePin to set the pinstate high or low depending on if the valve was intended to be open or closed. Similarly, the float sensor is a GPIO input read with HAL_GPIO_ReadPin to assess whether or not the pin registers as high or low. If it is high, (GPIO_PIN_SET), this indicates that the sensor is low and the valve must be opened.

For the TDS probe, more complex analysis had to occur. The TDS probe we purchased from DFRobot contained some open source code for converting the ADC readings to ppm readings, which we adapted with HAL_ADC_GetValue and HAL_ADC_PollForConversion to ensure it was operable with our MCU. We additionally allowed for UART transmission of the TDS probe sensor readings for manual validation of the values. Based on research, the TDS probe threshold was initially set at 250 ppm to trigger a valve opening, but due to the urgency of the demonstration, we modified this value to 150 ppm.

The motor is meant to operate on a timer system, which was initially difficult due to the necessity of nonblocking code. We ultimately settled on using a flag system and HAL_GetTicks which increments a counter. For the purpose of the demo, we settled on having the motor off for three minutes and on for six seconds. As HAL_Get_Ticks is constantly incremented as the main while loop runs, once it reaches a set value in milliseconds such as 6000 (six seconds) it modifies the flag to change the motor state.

UART and GPIO commands were primarily used for all drivers, leading to a cooperative multitasking system without blocking.

**Wireless connections**

Currently, the controller and plant board communicate with each other over Wi-Fi by sending AT commands to their own ESP-01 breakout boards. UART connections are used to issue the AT commands, which are all text strings. The plant board acts as a TCP/IP server and can only receive commands. There are 5 commands it can receive, all of which can be transmitted from the controller board. After the initialization and connection, the plant board runs as a single state machine, in which transitions between the state where it is receiving messages and the other states, in which it is operating the peripherals. The Wi-Fi credentials are hard coded in the program flashed to the plant board.

**Controller Board**

The controller board features buttons for user interaction and an LCD with LED bars for information display. The buttons use an enumeration and switch case logic to run a state machine. The button FSM takes care of debounce and long-press functionality if so desired. There are four button objects that are instantiated in the code. Below is a state diagram of the button class.

![Caption Text](c_fsm.png "Controller FSM/STD")

The LCD has an integrated display driver, enabling loading bits over 3-wire SPI bus. Images are read from a raw RGB565 array of a user defined image and sent in frames of 9 bits. The LED bars are composed of four vertical bars of four LEDs. They are addressable through shifting a 16 bit-bitmask into the LED control shift register and triggering the latch pin. This enables individual LED control from the MCU with only 3 logic pins.

**Troubleshooting**

**Plant Board**

Our initial version of the plant board became dysfunctional after an accidental hot-plugging incident (where the power was supplied to the 12V test point while the DC power supply was still outputting). Other factors that contributed to this destruction were the relays which control the valves. The issue was diagnosed to be, at minimum, an exploded LDO and a blown up MCU. Both components heated excessively and drew excess current. While testing without peripherals connected, the board was only able to draw 9V while hitting the current limit of 0.1A, which is much higher than expected. Previously, the board would draw 12V at 0.01A, showing that current draw increased by tenfold.

To assess the issue, power was supplied to the board, and each component was felt for temperature increases and each test point was probed with the intention of observing any suspicious shorts. Based on post-mortem damage analysis, the relay’s internal LED may have blown up due to a lack of resistor to reduce current input. The photo-mos relay was supplied with 3.3V instead of the required 2.3, potentially shorting both sides of the relay and supplying 12V to MCU. This is our working theory for why various board components were suddenly damaged.

![Caption Text](relay_pins.png "From the datasheet: a pinout of the relay")


The solution, as the complete issue was not diagnosed, was to assemble a secondary board. Based on the relay LED drawing 10mA and the MCU supplying 3.3V, a 100Ω resistor was bodged between the relay pin and pad. Since the MCU supplies commands to relay pin 1, we bent the pin up to avoid it contacting the pad and soldered a resistor vertically to contact the pad on one side and the pin on the other side, as shown in the image.

![Caption Text](relay_bodge.png "Bodging")


However, the new board was not without issues. Initially, the board was observed only to deliver 3.8V on the expected 5V line. 12V and 3.3V were drawing correct voltages and power, but 5V was shown to current limit. We then probed each component in the buck converter (from 12V to 5V) to assess if a passive component was broken, or if the buck itself was broken. This yielded no results. Due to randomness in the universe, the board magically started working after a solder blob was flicked off the inductor. Yippee!

Due to an act of God at the eleventh hour, our board does not accept code and has all read/data protections enabled, leading to the chip being impossible to chip erase or flash any code. We attempted to disable this protection for hours to no avail, and it seems the only solution is to replace the MCU.

**Controller Board**

During button testing, we realized that the buttons were oriented wrong by 90 degrees, leading to dysfunctionality. The internal wiring diagram did not match the footprint of the button device as provided with the manufacturer, so the buttons were permanently pulled high instead of being low while not pressed.

Additionally, due to a mismatch in net-naming between 3V3, +3.3V, and 3.3V, one of the pins on the LCD driver which needed to be powered to 3.3V was instead left floating. 32AWG PTFE wire was used to bridge between another powered LCD pin and the floating pin, which resolved the issue.

![Caption Text](controller_bodge.jpg "Controller Bodge")

The serial clock on the LCD was also routed to the wrong pin on the MCU. Due to the correct MCU pin and incorrect MCU pin being adjacent to each other (as well as both not requiring usage), both pins were bridged together with solder to allow correct signals to flow to the correct pin. The CUBEMX file was also updated to ensure the incorrect pin was not used.

**Final Product**

Well… it doesn’t work anymore. Here are some videos we took while it was! This video consists of clips of each individual functionality (sensors and actuators) as well as a video of the autonomous mode where the tank self-regulates.

https://youtu.be/ZvsrLwSTyi8




**Bill Of Materials**

 Controller Board BOM

| Designators | Qty | Comment | Footprint | LCSC PN | DigiKey PN | Alt DigiKey PN |
| --- | --- | --- | --- | --- | --- | --- |
| B1, B2, B3, B4 | 4 | TPS645-5NC105 | SOP450P919X345-4N | C19191549 |  |  |
| BOOT0 | 1 | STM32F429VGT6TR-CONTROLLERSTM32F429VGT6TR_PACKAGE | STM32F429VGT6TR_PACKAGE | C2052958 |  |  |
| C1, C4, C5, C6, C7, C8, C12, C13, C14, C15, C16, C18 | 12 | 100nF | "0805" | C1711 |  |  |
| C2, C3 | 2 | 4.7uF | "0805" | C1779 |  |  |
| C9, C10, C11, C17, C19 | 5 | 1uF | "0805" | C28323 |  |  |
| ESP8622 | 1 | ESP8266_ESP-01HDRVR8W64P254_2X4_1016X508X254B | HDRVR8W64P254_2X4_1016X508X254B |  | https://www.amazon.com/dp/B010N1ROQS?ref=ppx_yo2ov_dt_b_fed_asin_title |  |
| J1 | 1 | UX60SC-MB-5ST | UX60SC-MB-5ST |  | WM17116CT-ND |  |
| J2 | 1 | 2828XX-2282837-2 | TERMBLK_508-2N |  | A98333-ND |  |
| JP1 | 1 | PINHD-1X6 | 1X06 |  | SAM1029-06-ND |  |
| JP2 | 1 | PINHD-1X6 | 1X06 |  |  |  |
| R1, R2, R3, R4, R27, R30, R31, R32 | 8 | 10K | RESC2012X65 | C17414 |  |  |
| R5, R6, R7, R8, R9, R10, R11, R12, R16, R17, R18, R19, R20, R21, R22, R23 | 16 | 100 | RESC2012X65 | C17408 |  |  |
| R13, R14 | 2 | 10K | RESC1005X40 | C17414 |  |  |
| R15, R24, R25, R26 | 4 | 10 | RESC2012X65 | C17415 |  |  |
| R28 | 1 | 2K | RESC1005X40 | C17604 |  |  |
| S1, S2 | 2 | SPST-NO_SWITCH_KMR2 | KMR2 | C221675 |  |  |
| TP1, TP2, TP3, TP4, TP5, TP6 | 6 | TP_5005_COMPACT | TP-KEYSTONE-COMPACT |  |  | 36-5005-ND |
| U$2, U$3, U$4, U$5 | 4 | KB2450YWHDRVR8W50P254_1X8_2004X486X610B | HDRVR8W50P254_1X8_2004X486X610B | C17393516 |  |  |
| U$6 | 1 | IS31FL3726A-ZLS4-TR | SOP65P640X120-25T150X270N | C3190920 |  |  |
| U$7 | 1 | 522712069_RIBBONHDRRA_SMD_20S25P100_1X20_2200X670X300 | HDRRA_SMD_20S25P100_1X20_2200X670X300 | C3168975 |  |  |
| U$9 | 1 | AM4825P | SOIC127P600X175-8N | C5444618 |  | NTMS4177PR2GCT-ND |
| IC5 | 1 | ME6206A30M3G | SOT-23 | C487911 |  | 296-28778-1-ND |

 Main Board BOM

| Designators | Comment | Footprint | LCSC PN | DigiKey PN |
| --- | --- | --- | --- | --- |
| C1, C4 | 22u | 0805 | C45783 | N/A |
| C2, C5, C9, C10, C11, C12, C13, C14, C32, C33, C37, C38, C16, C18, C22, C26, C20, C24, C25, C28 | 100n | 0805 | CL21B104KBCNNNC | N/A |
| C3 | 10p | 0805 | CL10C100JB8NNNC | N/A |
| C6, C7, C15, C30, C31 | 1u | 0805 | CL21B105KBFNNNE | N/A |
| C8, C21, C23 | 4.7u | 0805 | CL21A475KAQNNNE | N/A |
| C15, C17, C27, C29, C19 | 10u | 0805 | CL21A106KAYNNNE | N/A |
| C36 | 1 n | 0805 | CL21C102JBCNNNC | N/A |
| D1 | CHIP-FLAT-R_0603-0.55MM | 0603 | C2286 | CP-036BHPJTR-ND |
| D2, D3 | DIODEDO-214AC(SMA) | DO-214AC | C84633 | N/A |
| D4, D5, D6, D7 | 1N4148WL | SOD-FL | C2908120 | N/A |
| D8 | 1N5819W | SOT-123FL | N/A | <br>1655-1N5819WTR-ND |
| D10 | CHIP-FLAT-R_0603-0.55MM | 0603 | C2286 | CP-036BHPJTR-ND |
| DRV1 | DRV8837 | WSON | C191000 | N/A |
| FB2 | BLM18PG121SN1D | 0603 | C14709 | N/A |
| FUSE1 | ERB-RG2R00V | 1 | N/A | P16739TR-ND |
| IC1 | TPS56425 | SOT-563 | C19191267 | N/A |
| IC2 | LMV324M | SOIC-14 | C529562 | N/A |
| IC3 | CD4060BM | SOIC-16 | C32699 | N/A |
| IC4 | TPS40600DBVR | SOT-753 | N/A | 296-27004-2-ND |
| IC5 | ME6206A30M3G | SOT-23 | C487911 | N/A |
| J1 | 548190572CONN_SD-54819-027_MOL |  | N/A |  |
| J2 | 2828XX-4282834-4 |  | N/A |  |
| J3, J6 | 2828XX-2282837-2 |  | N/A |  |
| J5 | 3-PIN SMD |  | N/A |  |
| JP1 | PINHD-1X6 |  | N/A |  |
| JP2 | JST-PH2 |  | N/A |  |
| L1 | 18m |  |  |  |
| LDO1 | TLV1117LV33DCYR | SOT-23 | N/A | <br>296-28778-2-ND |
| MCU1 | STM32F411CEU6 | QFN-49 | C60420 | N/A |
| PFET1 | AM4825P | SOIC-8 | N/A | <br>5318-AM4825P-ND |
| R1, R4, R9, R10, R11, R16, R17, R18, R19, R20, R21, R23, R24, R26 | 10K | 0805 | C17414 | N/A |
| R2 | 210k | 0805 | C17547 | N/A |
| R3, R6 | 30k | 0805 | C17621 | N/A |
| R5 | 220k | 0805 | C17556 | N/A |
| R7 | 120 | 0805 | C17437 | N/A |
| R8, R22 | 10 | 0805 | C17415 | N/A |
| R12, R25 | 100K | 0805 | C149504 | N/A |
| R13 | 6.8K | 0805 | C17772 | N/A |
| R14 | 1M | 0805 | C17514 | N/A |
| R15, R27 | 5.6K | 0805 | C4382 | N/A |
| RELAY1, RELAY2 | AQV101AZ |  | N/A | 255-2105-2-ND |
| S1 | SPST-NO_SWITCH_KMR2 |  |  |  |
| S2 | SPDT_SWITCH_CL-SB-12B-01T |  |  |  |
| TP1, TP2, TP3, TP4, TP5, TP6, TP7, TP8 | TP_5005_COMPACT |  | N/A | 36-5005-ND |
| U$1 | PJ-036BH-SMT-TRPJ-036BH-SMT-TR |  | N/A | CP-036BHPJTR-ND |
| WI1 | ESP8266_ESP-01HDRVR8W64P254_2X4_1016X508X254B |  | N/A |  |