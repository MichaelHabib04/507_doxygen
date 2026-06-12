/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.cpp
  * @brief          : Main program body for the STM32 nutrient/water controller
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
extern "C" {
#include "motor_driver.h"
#include "valve.h"
#include "float.h"
#include "probe.h"
}

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using std::string;
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/**
 * @brief High-level actions that can be requested by the TCP controller.
 */
enum class Actions{
	/** @brief Add water until the float switch indicates the target level. */
	Add_Water,

	/** @brief Add nutrient solution/slop and report the current TDS value. */
	Add_Slop,

	/** @brief Return control to manual/controller-command mode. */
	Controller_Mode,

	/** @brief Automatically manage water level and TDS concentration. */
	Automatic_Mode,

	/** @brief Idle state while waiting for the next TCP command. */
	Waiting_For_Command
};
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/** @brief TDS threshold, in ppm, below which nutrient solution is added. */
#define ppm_threshold 100.0f
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/** @brief PWM motor object used for periodic aeration. */
motor_t motor;

/** @brief Relay valve used to add water. */
valve_t valve1;

/** @brief Relay valve used to add nutrient solution/slop. */
valve_t valve2;

/** @brief Float switch used to detect the tank water level. */
float_t float1;

/** @brief TDS probe state used for ADC sampling and UART reporting. */
tds_t tds1;

/** @brief Nonzero when the aeration motor should be running. */
uint8_t motor_enable = 0;

/** @brief HAL tick count for the end of the previous aeration cycle. */
uint32_t last_motor_cycle_tick = 0;

/** @brief HAL tick count when the current aeration run started. */
uint32_t  motor_start_tick = 0;

/** @brief Latest TDS measurement in parts per million. */
float ppm;

/** @brief True after the remote controller sends the pairing message. */
bool controller_paired = false;

/** @brief True when automatic tank-control behavior is enabled. */
bool automatic_mode = false;

/** @brief Current state-machine action being processed by the main loop. */
Actions current_action = Actions::Waiting_For_Command;

/* WiFi credentials ----------------------------------------------------------*/
//static const char WIFI_SSID[] = "JiyenPhone";
//static const char WIFI_PASS[] = "loquat1515";
static const char WIFI_SSID[] = "LucasPhone";
static const char WIFI_PASS[] = "w033qbwyu552";


/* TCP server/listener port --------------------------------------------------*/
/* Change this to the port you want the ESP-01 to listen on. */
/** @brief TCP port opened by the ESP-01 server for controller commands. */
static const int LISTEN_PORT = 5000;

/* RX buffer for ESP UART ----------------------------------------------------*/
/** @brief Size of the circular receive buffer used for ESP UART data. */
#define ESP_RX_BUF_SIZE 1024

/** @brief Circular buffer that stores bytes received from the ESP-01 UART. */
static uint8_t esp_rx_buf[ESP_RX_BUF_SIZE];

/** @brief Write index for the ESP UART circular receive buffer. */
static uint16_t esp_rx_head = 0;

/** @brief Read index for the ESP UART circular receive buffer. */
static uint16_t esp_rx_tail = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
/**
 * @brief Push one byte into the ESP UART circular receive buffer.
 *
 * If the buffer is full, the oldest byte is overwritten.
 *
 * @param b Byte to store in the receive buffer.
 */
static void rx_push(uint8_t b)
{
  esp_rx_buf[esp_rx_head] = b;
  esp_rx_head = (esp_rx_head + 1) % ESP_RX_BUF_SIZE;

  if (esp_rx_head == esp_rx_tail)
  {
    esp_rx_tail = (esp_rx_tail + 1) % ESP_RX_BUF_SIZE;
  }
}

/**
 * @brief Pop one byte from the ESP UART circular receive buffer.
 *
 * @return Next byte as a non-negative int, or -1 when the buffer is empty.
 */
static int rx_pop(void)
{
  if (esp_rx_head == esp_rx_tail)
  {
    return -1;
  }

  uint8_t b = esp_rx_buf[esp_rx_tail];
  esp_rx_tail = (esp_rx_tail + 1) % ESP_RX_BUF_SIZE;
  return b;
}

/**
 * @brief Read all currently available bytes from the ESP UART.
 *
 * Bytes are pushed into the local circular receive buffer until no more data is
 * available or a HAL receive error occurs.
 */
static void esp_fill_rx(void)
{
  uint8_t b;
  HAL_StatusTypeDef status;

  do
  {
    status = HAL_UART_Receive(&huart2, &b, 1, 1);

    if (status == HAL_OK)
    {
      rx_push(b);
    }
  } while (status == HAL_OK);
}

/**
 * @brief Clear buffered ESP UART data.
 *
 * The circular buffer indexes are reset and any pending UART bytes are drained
 * so old responses are not confused with new AT command responses.
 */
static void esp_clear_rx(void)
{
  esp_rx_head = 0;
  esp_rx_tail = 0;

  uint8_t b;

  while (HAL_UART_Receive(&huart2, &b, 1, 1) == HAL_OK)
  {
  }
}

/* UART1 debug helpers -------------------------------------------------------*/
/**
 * @brief Send a null-terminated debug string over UART1.
 *
 * @param msg Null-terminated string to transmit.
 */
static void uart1_send(const char *msg)
{
  HAL_UART_Transmit(&huart1,
                    (uint8_t *)msg,
                    (uint16_t)std::strlen(msg),
                    HAL_MAX_DELAY);
}

/**
 * @brief Send a null-terminated debug string over UART1 with CRLF appended.
 *
 * @param msg Null-terminated string to transmit.
 */
static void uart1_send_ln(const char *msg)
{
  uart1_send(msg);
  uart1_send("\r\n");
}

/**
 * @brief Send a C++ string over UART1.
 *
 * @param msg String to transmit.
 */
static void uart1_send_string(const string &msg)
{
  HAL_UART_Transmit(&huart1,
                    (uint8_t *)msg.c_str(),
                    (uint16_t)msg.length(),
                    HAL_MAX_DELAY);
}

/**
 * @brief Send a C++ string over UART1 with CRLF appended.
 *
 * @param msg String to transmit.
 */
static void uart1_send_string_ln(const string &msg)
{
  uart1_send_string(msg);
  uart1_send("\r\n");
}

/* UART2 ESP send helper -----------------------------------------------------*/
/**
 * @brief Send a C++ string over UART2 with CRLF appended.
 *
 * This helper is used to send ESP-01 AT commands.
 *
 * @param msg AT command or string to transmit.
 */
static void uart2_send_string_ln(const string &msg)
{
  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg.c_str(),
                    (uint16_t)msg.length(),
                    HAL_MAX_DELAY);

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)"\r\n",
                    2,
                    HAL_MAX_DELAY);
}

/**
 * @brief Send an AT command to the ESP-01 and collect its response.
 *
 * The function clears pending input, sends the command, then reads bytes until
 * an expected response terminator appears or the timeout expires. The response
 * is also printed over UART1 for debugging.
 *
 * @param cmd AT command to send without the trailing CRLF.
 * @param timeout_ms Maximum time to wait for the ESP response, in milliseconds.
 * @return Complete response collected from the ESP-01.
 */
static string esp_command(const string &cmd, uint32_t timeout_ms)
{
  esp_clear_rx();

  uart1_send(">> ");
  uart1_send_string_ln(cmd);

  uart2_send_string_ln(cmd);

  string response;
  response.reserve(ESP_RX_BUF_SIZE);

  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout_ms)
  {
    esp_fill_rx();

    int r = rx_pop();

    if (r >= 0)
    {
      char c = (char)r;
      response += c;

      if (response.find("\r\nOK\r\n") != string::npos ||
          response.find("\r\nERROR\r\n") != string::npos ||
          response.find("FAIL") != string::npos ||
          response.find("ALREADY CONNECTED") != string::npos)
      {
        break;
      }
    }
    else
    {
      HAL_Delay(1);
    }
  }

  uart1_send_ln("<< ESP response:");
  uart1_send_string_ln(response);
  uart1_send_ln("--------------------");

  return response;
}

/**
 * @brief Parse the station IP address from an AT+CIPSTA? response.
 *
 * @param response Raw ESP-01 response string.
 * @return Parsed IP address string, or an empty string if not found.
 */
static string parse_cipsta_ip(const string &response)
{
  size_t pos = response.find("+CIPSTA:ip:\"");

  if (pos == string::npos)
  {
    return "";
  }

  size_t q1 = response.find('"', pos);
  if (q1 == string::npos)
  {
    return "";
  }

  size_t q2 = response.find('"', q1 + 1);
  if (q2 == string::npos)
  {
    return "";
  }

  return response.substr(q1 + 1, q2 - q1 - 1);
}

/**
 * @brief Parse the station IP address from an AT+CIFSR response.
 *
 * @param response Raw ESP-01 response string.
 * @return Parsed IP address string, or an empty string if not found.
 */
static string parse_cifsr_sta_ip(const string &response)
{
  size_t pos = response.find("STAIP,\"");

  if (pos == string::npos)
  {
    return "";
  }

  size_t q1 = response.find('"', pos);
  if (q1 == string::npos)
  {
    return "";
  }

  size_t q2 = response.find('"', q1 + 1);
  if (q2 == string::npos)
  {
    return "";
  }

  return response.substr(q1 + 1, q2 - q1 - 1);
}

/**
 * @brief Try to read the ESP-01 station IP address once.
 *
 * The function first attempts AT+CIPSTA?, then falls back to AT+CIFSR.
 *
 * @return Parsed IP address string, or an empty string if no IP was found.
 */
static string get_ip_address_once(void)
{
  string response;
  string ip;

  response = esp_command("AT+CIPSTA?", 5000);
  ip = parse_cipsta_ip(response);

  if (ip.empty())
  {
    response = esp_command("AT+CIFSR", 5000);
    ip = parse_cifsr_sta_ip(response);
  }

  return ip;
}

/**
 * @brief Wait until the ESP-01 has an IP address and print it over UART1.
 */
static void wait_for_ip_and_print_once(void)
{
  string ip = "";

  uart1_send_ln("");
  uart1_send_ln("Waiting for ESP IP address...");

  while (ip.empty())
  {
    ip = get_ip_address_once();

    if (ip.empty())
    {
      uart1_send_ln("IP not found yet. Retrying...");
      HAL_Delay(2000);
    }
  }

  uart1_send("ESP IP Address: ");
  uart1_send_string_ln(ip);
  uart1_send_ln("Use this IP address from your computer TCP client.");
}

/**
 * @brief Configure the ESP-01 and connect it to the configured WiFi network.
 *
 * @return true if the ESP-01 connects and reports success; false otherwise.
 */
static bool wifi_connect(void)
{
  uart1_send_ln("");
  uart1_send_ln("Starting ESP-01 WiFi setup...");
  HAL_Delay(2000);

  string response;

  response = esp_command("AT", 3000);

  if (response.find("OK") == string::npos)
  {
    uart1_send_ln("ERROR: ESP-01 did not respond to AT.");
    uart1_send_ln("Check wiring, power, EN/CH_PD, GPIO0, GPIO2, baud rate, and TX/RX crossover.");
    return false;
  }

  esp_command("ATE0", 2000);

  esp_command("AT+RST", 6000);
  HAL_Delay(4000);

  response = esp_command("AT+CWMODE=1", 5000);

  if (response.find("OK") == string::npos)
  {
    uart1_send_ln("ERROR: Failed to set station mode.");
    return false;
  }

  esp_command("AT+CWDHCP=1,1", 5000);
  esp_command("AT+CIPMODE=0", 3000);

  esp_command("AT+CWQAP", 5000);
  HAL_Delay(1000);

  uart1_send_ln("");
  uart1_send_ln("Connecting to WiFi...");

  string join_cmd = "AT+CWJAP=\"";
  join_cmd += WIFI_SSID;
  join_cmd += "\",\"";
  join_cmd += WIFI_PASS;
  join_cmd += "\"";

  response = esp_command(join_cmd, 30000);

  if (response.find("OK") != string::npos ||
      response.find("WIFI GOT IP") != string::npos)
  {
    uart1_send_ln("WiFi connected.");
    return true;
  }

  uart1_send_ln("WiFi connection failed.");

  if (response.find("+CWJAP:1") != string::npos)
  {
    uart1_send_ln("CWJAP error 1: connection timeout.");
  }
  else if (response.find("+CWJAP:2") != string::npos)
  {
    uart1_send_ln("CWJAP error 2: wrong password.");
  }
  else if (response.find("+CWJAP:3") != string::npos)
  {
    uart1_send_ln("CWJAP error 3: access point not found.");
  }
  else if (response.find("+CWJAP:4") != string::npos)
  {
    uart1_send_ln("CWJAP error 4: connection failed.");
  }

  return false;
}

/**
 * @brief Start the ESP-01 TCP server/listener.
 *
 * @param listen_port TCP port number for incoming controller connections.
 * @return true if the server starts successfully; false otherwise.
 */
static bool start_tcp_server(int listen_port)
{
  string response;

  uart1_send_ln("");
  uart1_send_ln("Starting ESP TCP server/listener...");

  /*
    Server mode requires multiple connections enabled.
  */
  response = esp_command("AT+CIPMUX=1", 5000);

  if (response.find("OK") == string::npos)
  {
    uart1_send_ln("ERROR: Failed to set CIPMUX=1.");
    return false;
  }

  /*
    Stop any old server. Ignore ERROR if no server exists.
  */
  esp_command("AT+CIPSERVER=0", 3000);

  char server_cmd[48];
  std::snprintf(server_cmd,
           sizeof(server_cmd),
           "AT+CIPSERVER=1,%d",
           listen_port);

  response = esp_command(server_cmd, 5000);

  if (response.find("OK") == string::npos)
  {
    uart1_send_ln("ERROR: Failed to start TCP server.");
    return false;
  }

  /*
    Optional connection timeout in seconds.
    7200 seconds = 2 hours.
  */
  esp_command("AT+CIPSTO=7200", 3000);

  uart1_send("TCP server listening on port ");

  char port_buf[16];
  std::snprintf(port_buf, sizeof(port_buf), "%d", listen_port);
  uart1_send_ln(port_buf);

  return true;
}

/**
 * @brief Test whether a character is an ASCII decimal digit.
 *
 * @param c Character to test.
 * @return true when c is between '0' and '9'; false otherwise.
 */
static bool is_digit_char(char c)
{
  return c >= '0' && c <= '9';
}

/**
 * @brief Parse an unsigned integer from a string at the current position.
 *
 * @param s Input string containing the number.
 * @param pos Current parse position; updated to the first non-digit on success.
 * @param value Parsed unsigned value.
 * @return true if at least one digit was parsed; false otherwise.
 */
static bool parse_unsigned_from_stream(const string &s,
                                       size_t &pos,
                                       unsigned int &value)
{
  if (pos >= s.length() || !is_digit_char(s[pos]))
  {
    return false;
  }

  value = 0;

  while (pos < s.length() && is_digit_char(s[pos]))
  {
    value = value * 10U + (unsigned int)(s[pos] - '0');
    pos++;
  }

  return true;
}

/**
 * @brief Extract one complete TCP payload from an ESP +IPD stream.
 *
 * Expected ESP server/mux format:
 * @code
 * +IPD,<link_id>,<length>:<payload>
 * @endcode
 *
 * The non-mux format is also supported:
 * @code
 * +IPD,<length>:<payload>
 * @endcode
 *
 * @param stream Buffered ESP receive stream. Consumed bytes are erased.
 * @return Received TCP payload, or "None" if no complete payload is available.
 */
static string process_ipd_stream(string &stream)
{
  while (1)
  {
    size_t ipd_pos = stream.find("+IPD,");

    if (ipd_pos == string::npos)
    {
      /*
        Keep a small tail in case '+IPD,' arrives split across reads.
      */
      if (stream.length() > 8)
      {
        stream.erase(0, stream.length() - 8);
      }

      return "None";
    }

    if (ipd_pos > 0)
    {
      stream.erase(0, ipd_pos);
    }

    size_t pos = 5;
    unsigned int first_number = 0;
    unsigned int payload_len = 0;

    if (!parse_unsigned_from_stream(stream, pos, first_number))
    {
      return "None";
    }

    if (pos >= stream.length())
    {
      return "None";
    }

    if (stream[pos] == ',')
    {
      /*
        Mux/server format:
          +IPD,<link_id>,<length>:<payload>
      */
      pos++;

      if (!parse_unsigned_from_stream(stream, pos, payload_len))
      {
        return "None";
      }

      if (pos >= stream.length())
      {
        return "None";
      }

      if (stream[pos] != ':')
      {
        /*
          Unexpected format. Drop '+IPD,' and keep searching.
        */
        stream.erase(0, 5);
        continue;
      }

      pos++;
    }
    else if (stream[pos] == ':')
    {
      /*
        Non-mux/client format:
          +IPD,<length>:<payload>
      */
      payload_len = first_number;
      pos++;
    }
    else
    {
      /*
        Unexpected format. Drop '+IPD,' and keep searching.
      */
      stream.erase(0, 5);
      continue;
    }

    if (stream.length() < pos + payload_len)
    {
      /*
        Full payload has not arrived yet.
      */
      return "None";
    }

    string payload = stream.substr(pos, payload_len);

    /*
      Remove this packet from the stream.
      If another +IPD packet is already buffered, the next call will return it.
    */
    stream.erase(0, pos + payload_len);

    return payload;
  }
}

/**
 * @brief Check whether a complete TCP command has been received.
 *
 * This function should be called from the main loop. It reads available UART2
 * bytes, stores them in a persistent stream buffer, and extracts one complete
 * ESP +IPD payload when available.
 *
 * @return Received TCP message payload, or "None" if no complete message exists.
 */
static string check_for_received_tcp_messages(void)
{
  static string stream;

  esp_fill_rx();

  while (1)
  {
    int r = rx_pop();

    if (r < 0)
    {
      break;
    }

    stream += (char)r;

    if (stream.length() > 2048)
    {
      stream.erase(0, stream.length() - 2048);
    }
  }

  return process_ipd_stream(stream);
}


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief Application entry point.
  *
  * Initializes peripherals, connects the ESP-01 to WiFi, waits for controller
  * pairing, then repeatedly processes water, nutrient, automatic-mode, and
  * aeration control logic.
  *
  * @retval int This function does not return during normal operation.
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();


  /* USER CODE BEGIN 2 */
  motor = motor_t{TIM_CHANNEL_1, TIM_CHANNEL_2, &htim1};
  motor_chan_enable(&motor);
  motor_set_duty_cycle(&motor, 0);
  valve1 = valve_t{Relay1_GPIO_Port, Relay1_Pin};
  valve2 = valve_t{Relay2_GPIO_Port, Relay2_Pin};
  float1 = float_t{Float_GPIO_Port, Float_Pin};
  tds_init(&tds1, &hadc1, &huart1, 3.3f);
  last_motor_cycle_tick = HAL_GetTick();


  // Connect to WiFi and start TCP server/listener for controller pairing and commands.
  uart1_send_ln("");
    uart1_send_ln("===== TCP LISTENER BUILD 2026-06-11 v2 RETURN STRING =====");
    uart1_send_ln("");

    while (!wifi_connect())
    {
      uart1_send_ln("");
      uart1_send_ln("WiFi setup failed. Retrying in 5 seconds...");
      HAL_Delay(5000);
    }

    wait_for_ip_and_print_once();

    while (!start_tcp_server(LISTEN_PORT))
    {
      uart1_send_ln("TCP server setup failed. Retrying in 5 seconds...");
      HAL_Delay(5000);
    }

    uart1_send_ln("");
    uart1_send_ln("Ready. Main loop is checking for received TCP messages.");

    while (!controller_paired)
    {
  	uart1_send_ln("");
  	uart1_send_ln("Waiting for controller to pair...");

  	string received_message = check_for_received_tcp_messages();
  	if (received_message == "Hello")
  	{
      	controller_paired = true;
  		uart1_send_ln("Controller paired successfully!");
  		break;
  	}
  	HAL_Delay(1000);
    }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    uint32_t now = HAL_GetTick();

    switch (current_action)
    {
      case Actions::Add_Water:
      {
        uart1_send_ln("Action: Add Water");

        // If the float switch is not pressed, open valve1 to add water.
        // Otherwise, close valve1 and return to waiting for a command.
        if (read_float(&float1) == GPIO_PIN_SET)
        {
          valve_open(&valve1);
          HAL_Delay(100);
          valve_close(&valve1);
          current_action = Actions::Add_Water;
        }
        else
        {
          valve_close(&valve1);
          current_action = Actions::Waiting_For_Command;
        }
        break;
      }

      case Actions::Add_Slop:
      {
        uart1_send_ln("Action: Add Slop");

        ppm = tds_read_ppm(&tds1);
        tds_uart_print(&tds1);
        valve_open(&valve2);
        HAL_Delay(1000);
        valve_close(&valve2);

        current_action = Actions::Waiting_For_Command;
        break;
      }

      case Actions::Controller_Mode:
      {
        uart1_send_ln("Action: Controller Mode");
        automatic_mode = false;
        current_action = Actions::Waiting_For_Command;
        break;
      }

      case Actions::Automatic_Mode:
      {
        uart1_send_ln("Action: Automatic Mode");

        if (read_float(&float1) == GPIO_PIN_SET)
        {
          valve_open(&valve1);
        }
        else
        {
          valve_close(&valve1);
        }

        ppm = tds_read_ppm(&tds1);
        tds_uart_print(&tds1);

        if (ppm <= ppm_threshold)
        {
          valve_open(&valve2);
        }
        else
        {
          valve_close(&valve2);
        }

        automatic_mode = true;
        current_action = Actions::Waiting_For_Command;
        break;
      }

      case Actions::Waiting_For_Command:
      default:
      {
        // Check for the next command from the controller over TCP.
        string received_message = check_for_received_tcp_messages();

        if (received_message != "None")
        {
          uart1_send_ln("");
          uart1_send_ln("Received TCP message:");
          uart1_send_string_ln(received_message);
          uart1_send_ln("--------------------");

          if (received_message == "AUTOM")
          {
            uart1_send_ln("Received AUTOM message.");
            current_action = Actions::Automatic_Mode;
          }
          else if (received_message == "WATER")
          {
            uart1_send_ln("Received WATER message.");
            current_action = Actions::Add_Water;
          }
          else if (received_message == "SLOP!")
          {
            uart1_send_ln("Received SLOP! message.");
            current_action = Actions::Add_Slop;
          }
          else if (received_message == "CTRLR")
          {
            uart1_send_ln("Received CTRLR message.");
            current_action = Actions::Controller_Mode;
            automatic_mode = false;
          }
        }
        break;
      }
    }

    // Regardless of the current mode, always aerate the water with the same cycle.
    if ((motor_enable == 0) && ((now - last_motor_cycle_tick) >= 36000UL))
    {
      motor_enable = 1;
      motor_start_tick = now;
    }

    if ((motor_enable == 1) && ((now - motor_start_tick) >= 6000UL))
    {
      motor_enable = 0;
      last_motor_cycle_tick = now;
    }

    if (motor_enable)
    {
      motor_set_duty_cycle(&motor, 100);
    }
    else
    {
      motor_stop(&motor);
    }

    HAL_Delay(50);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief Configure the STM32 system clock tree.
  *
  * The firmware uses the internal HSI oscillator and PLL configuration generated
  * by STM32CubeIDE.
  *
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Initialize ADC1 for single-channel TDS probe sampling.
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief Initialize TIM1 for two-channel PWM motor control.
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 4799;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief Initialize USART1 for debug output.
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief Initialize USART2 for ESP-01 AT command communication.
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief Initialize GPIO pins for relays, float switch, and board outputs.
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, Relay1_Pin|Relay2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA4 PA5 PA6 PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : Float_Pin */
  GPIO_InitStruct.Pin = Float_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Float_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Relay1_Pin Relay2_Pin */
  GPIO_InitStruct.Pin = Relay1_Pin|Relay2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

