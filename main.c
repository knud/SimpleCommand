/**
 * Copyright (c) 2015 - 2018, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*!
 * @brief Simple BLE command main file based on the Nordic UART service.
 *
 * @details This has been adapted for use without the UART. Commands are sent by a central
 * to a peripheral and answered.
 *
 * @author Steven Knudsen
 * @date 2019-03-22
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_err.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "boards.h"
#include "app_timer.h"
#include "ble_lbs.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_delay.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ble_cmd.h"
#include "command.h"

#define ADVERTISING_LED                 BSP_BOARD_LED_0                         // Is on when device is advertising.
#define CONNECTED_LED                   BSP_BOARD_LED_1                         // Is on when device has connected.
#define BLINK_LED_1                     BSP_BOARD_LED_2                         // A LED that responds to commands
#define BLINK_LED_2                     BSP_BOARD_LED_3                         // A LED that responds to commands

#define DEVICE_NAME                     "Peripheral1"                           // Name of device. Will be included in the advertising data.

#define CMD_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN              // UUID type for the Command Service (vendor specific).

#define APP_BLE_OBSERVER_PRIO           3                                       // Application's BLE observer priority. You shouldn't need to modify this value.
#define APP_BLE_CONN_CFG_TAG            1                                       // A tag identifying the SoftDevice BLE configuration.

#define APP_ADV_INTERVAL                64                                      // The advertising interval (in units of 0.625 ms; this value corresponds to 40 ms).
#define APP_ADV_DURATION                BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED   // The advertising time-out (in units of seconds). When set to 0, we will never time out.


#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)        // Minimum acceptable connection interval (0.5 seconds).
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)        // Maximum acceptable connection interval (1 second).
#define SLAVE_LATENCY                   0                                       // Slave latency.
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)         // Connection supervisory time-out (4 seconds).

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(20000)                  // Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (15 seconds).
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(5000)                   // Time between each call to sd_ble_gap_conn_param_update after the first call (5 seconds).
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                       // Number of attempts before giving up the connection parameter negotiation.

#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50)                     // Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks).

#define DEAD_BEEF                       0xDEADBEEF                              // Value used as error code on stack dump, can be used to identify stack location on stack unwind.


BLE_LBS_DEF(m_lbs);                                                             // LED Button Service instance.
NRF_BLE_GATT_DEF(m_gatt);                                                       // GATT module instance.
NRF_BLE_QWR_DEF(m_qwr);                                                         // Context for the Queued Write module

//static bool connected;

static command_id_t m_currentCommand;
static bool m_connected;

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;                        // Handle of the current connection.
static uint16_t   m_ble_cmd_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;        // Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module.
static ble_uuid_t m_adv_uuids[]          =                                      // Universally unique service identifier.
{
    {BLE_UUID_CMD_SERVICE, CMD_SERVICE_UUID_TYPE}
};

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;                   // Advertising handle used to identify an advertising set.
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];                    // Buffer for storing an encoded advertising set.
static uint8_t m_enc_scan_response_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX];         // Buffer for storing an encoded scan data.

/**@brief Struct that contains pointers to the encoded advertising data. */
static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_enc_advdata,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = m_enc_scan_response_data,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX

    }
};

/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
  app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by the application.
 */
static void leds_init(void)
{
  bsp_board_init(BSP_INIT_LEDS);
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
  // Initialize timer module, making it use the scheduler
  ret_code_t err_code = app_timer_init();
  APP_ERROR_CHECK(err_code);
}


/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
  ret_code_t              err_code;
  ble_gap_conn_params_t   gap_conn_params;
  ble_gap_conn_sec_mode_t sec_mode;

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

  err_code = sd_ble_gap_device_name_set(&sec_mode,
      (const uint8_t *)DEVICE_NAME,
      strlen(DEVICE_NAME));
  APP_ERROR_CHECK(err_code);

  memset(&gap_conn_params, 0, sizeof(gap_conn_params));

  gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
  gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
  gap_conn_params.slave_latency     = SLAVE_LATENCY;
  gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

  err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
  APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
  NRF_LOG_INFO("gatt_evt_handler");
  if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
  {
    m_ble_cmd_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
    NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_cmd_max_data_len, m_ble_cmd_max_data_len);
  }
  NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
      p_gatt->att_mtu_desired_central,
      p_gatt->att_mtu_desired_periph);
}

/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void)
{
  //    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
  //    APP_ERROR_CHECK(err_code);
  ret_code_t err_code;

  err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
  APP_ERROR_CHECK(err_code);

  err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
  APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
  ret_code_t    err_code;
  ble_advdata_t advdata;
  ble_advdata_t srdata;

  //    ble_uuid_t adv_uuids[] = {{LBS_UUID_SERVICE, m_lbs.uuid_type}};

  // Build and set advertising data.
  memset(&advdata, 0, sizeof(advdata));

  advdata.name_type          = BLE_ADVDATA_FULL_NAME;
  advdata.include_appearance = true;
  advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;


  memset(&srdata, 0, sizeof(srdata));
  srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
  srdata.uuids_complete.p_uuids  = m_adv_uuids;

  err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
  APP_ERROR_CHECK(err_code);

  err_code = ble_advdata_encode(&srdata, m_adv_data.scan_rsp_data.p_data, &m_adv_data.scan_rsp_data.len);
  APP_ERROR_CHECK(err_code);

  ble_gap_adv_params_t adv_params;

  // Set advertising parameters.
  memset(&adv_params, 0, sizeof(adv_params));

  adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;
  adv_params.duration        = APP_ADV_DURATION;
  adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
  adv_params.p_peer_addr     = NULL;
  adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
  adv_params.interval        = APP_ADV_INTERVAL;

  err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &adv_params);
  APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
  APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for handling the data from the Command Service.
 *
 * @details This function will process the data received from the Client and
 * send to the raw command processor. If the command is valid, execute it.
 *
 * @param[in] p_evt Service event.
 */
static void cmd_data_handler(ble_cmd_evt_t * p_evt)
{
  NRF_LOG_INFO("cmd_data_handler enter");

  if (p_evt->type == BLE_CMD_EVT_RX_DATA)
  {
    NRF_LOG_INFO("Received %d chars",p_evt->params.rx_data.length);

    // TODO This is blocking. Is that okay?
    receiveRawCommand(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);

    if (validCommandReceived())
      executeCommand();
  }

}

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
  ret_code_t           err_code;
  nrf_ble_qwr_init_t qwr_init = {0};

  // Initialize Queued Write Module.
  qwr_init.error_handler = nrf_qwr_error_handler;

  err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
  APP_ERROR_CHECK(err_code);

  err_code = ble_cmd_init(cmd_data_handler, &m_conn_handle);
  APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module that
 *          are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply
 *       setting the disconnect_on_fail config parameter, but instead we use the event
 *       handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
  ret_code_t err_code;

  if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
  {
    err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
    APP_ERROR_CHECK(err_code);
  }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
  APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
  ret_code_t             err_code;
  ble_conn_params_init_t cp_init;

  memset(&cp_init, 0, sizeof(cp_init));

  cp_init.p_conn_params                  = NULL;
  cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
  cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
  cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
  cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
  cp_init.disconnect_on_fail             = false;
  cp_init.evt_handler                    = on_conn_params_evt;
  cp_init.error_handler                  = conn_params_error_handler;

  err_code = ble_conn_params_init(&cp_init);
  APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
  ret_code_t           err_code;

  err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
  APP_ERROR_CHECK(err_code);

  bsp_board_led_on(ADVERTISING_LED);
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
  ret_code_t err_code;

  switch (p_ble_evt->header.evt_id)
  {
  case BLE_GAP_EVT_CONNECTED:
    NRF_LOG_INFO("Connected");
    bsp_board_led_on(CONNECTED_LED);
    bsp_board_led_off(ADVERTISING_LED);
    m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
    APP_ERROR_CHECK(err_code);
    APP_ERROR_CHECK(err_code);
    m_connected = true;
    break;

  case BLE_GAP_EVT_DISCONNECTED:
    NRF_LOG_INFO("Disconnected");
    bsp_board_led_off(CONNECTED_LED);
    m_conn_handle = BLE_CONN_HANDLE_INVALID;
    advertising_start();
    m_connected = false;
    bsp_board_led_off(BLINK_LED_1);
    bsp_board_led_off(BLINK_LED_2);
    break;

  case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
    // Pairing not supported
    err_code = sd_ble_gap_sec_params_reply(m_conn_handle,
        BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
        NULL,
        NULL);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
  {
    NRF_LOG_DEBUG("PHY update request.");
    ble_gap_phys_t const phys =
    {
        .rx_phys = BLE_GAP_PHY_AUTO,
        .tx_phys = BLE_GAP_PHY_AUTO,
    };
    err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
    APP_ERROR_CHECK(err_code);
  } break;

  case BLE_GATTS_EVT_SYS_ATTR_MISSING:
    // No system attributes have been stored.
    err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_GATTC_EVT_TIMEOUT:
    // Disconnect on GATT Client timeout event.
    NRF_LOG_DEBUG("GATT Client Timeout.");
    err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
        BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_GATTS_EVT_TIMEOUT:
    // Disconnect on GATT Server timeout event.
    NRF_LOG_DEBUG("GATT Server Timeout.");
    err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
        BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    APP_ERROR_CHECK(err_code);
    break;

  default:
    // No implementation needed.
    break;
  }
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
  ret_code_t err_code;

  err_code = nrf_sdh_enable_request();
  APP_ERROR_CHECK(err_code);

  // Configure the BLE stack using the default settings.
  // Fetch the start address of the application RAM.
  uint32_t ram_start = 0;
  err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
  APP_ERROR_CHECK(err_code);

  // Enable BLE stack.
  err_code = nrf_sdh_ble_enable(&ram_start);
  APP_ERROR_CHECK(err_code);

  // Register a handler for BLE events.
  NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


static void log_init(void)
{
  ret_code_t err_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(err_code);

  NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
  ret_code_t err_code;
  err_code = nrf_pwr_mgmt_init();
  APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
  //  if (NRF_LOG_PROCESS() == false)
  //  {
  //    nrf_pwr_mgmt_run();
  //  }
}


/**@brief Function for application main entry.
 */
int main(void)
{
  m_currentCommand = NO_COMMAND;
  m_connected = false;
  // Initialize.
  log_init();
  leds_init();
  timers_init();
  //    buttons_init();
  power_management_init();
  ble_stack_init();
  gap_params_init();
  gatt_init();
  services_init();
  advertising_init();
  conn_params_init();

  // Start execution.
  NRF_LOG_INFO("Simple Command started.");
  advertising_start();

  // Enter main loop.
  for (;;)
  {
    idle_state_handle();
    if (m_connected) // TODO move into idle_state_handle
    {
      command_id_t cc = currentCommand();
      switch(cc)
      {
      case NO_COMMAND:
      case OFF:
      case ABORT:
        bsp_board_led_off(BLINK_LED_1);
        bsp_board_led_off(BLINK_LED_2);
        nrf_delay_ms(100);
        break;
      case FAST_BLINK:
        bsp_board_led_invert(BLINK_LED_1);
        bsp_board_led_off(BLINK_LED_2);
        nrf_delay_ms(50);
        break;
      case SLOW_BLINK:
        bsp_board_led_invert(BLINK_LED_2);
        bsp_board_led_off(BLINK_LED_1);
        nrf_delay_ms(250);
        break;
      case ALT_BLINK:
        bsp_board_led_on(BLINK_LED_1);
        bsp_board_led_off(BLINK_LED_2);
        nrf_delay_ms(250);
        bsp_board_led_off(BLINK_LED_1);
        bsp_board_led_on(BLINK_LED_2);
        nrf_delay_ms(250);
        break;
      default:
        bsp_board_led_off(BLINK_LED_1);
        bsp_board_led_off(BLINK_LED_2);
        nrf_delay_ms(100);
        break;
      }
    } else
    {
      nrf_delay_ms(100);
    }
  }
}


/**
 * @}
 */
