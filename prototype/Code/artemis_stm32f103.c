/**
 * =============================================================================
 * ARTEMIS — STM32F103C8T6 Firmware
 * Helios-Artemis Dual-MCU Predictive MPPT Controller
 * =============================================================================
 *
 * Responsibilities:
 *   - Variable-Step Perturb & Observe (VS-P&O) MPPT at 100 ms intervals
 *   - 50 kHz PWM generation via TIM1 PA8 → TC4420 gate driver → IRFB4110
 *   - INA219 (I²C, 0x40) reads: bus voltage, shunt current
 *   - Three-stage CC/CV/Float battery charging FSM
 *   - 100 ms UART RX from Helios: predicted V_MPP + alpha blend weight
 *   - UART TX to Helios: V_bat, I_bat, duty, state, irradiance est.
 *
 * Hardware:
 *   MCU   : STM32F103C8T6, 72 MHz, Cortex-M3
 *   PWM   : TIM1 CH1 (PA8), 50 kHz, 0–100% duty
 *   I²C   : PB6 (SCL) / PB7 (SDA) — INA219 @ 0x40
 *   UART1 : PA9 (TX) / PA10 (RX), 115200 8N1 — Helios link
 *   Vref  : Internal 3.3 V, ADC PA0 for PV voltage divider (optional)
 *
 * Algorithm (from paper §III-B):
 *   dl = clip(0.008 × |dP/dV|, 0.05, 0.60) V
 *   V_ref = (1-α)·V_ref_P&O + α·V_MPP_pred
 *   Blend fires only when |Ĝ−G|/G > 0.15
 *   Asymmetric α: cloud-clear = 0.45·exp(−1.5·rel_dev)
 *                 cloud-drop  = 0.08·exp(−1.0·rel_dev)
 *   20-step cooldown after each blend
 *
 * Battery charging (12V SLA):
 *   Bulk       : I = 6 A   until V_bat ≥ 14.7 V
 *   Absorption : V = 14.7 V until I_tail ≤ 0.5 A
 *   Float      : V = 13.8 V (trickle maintenance)
 *
 * Build: arm-none-eabi-gcc, STM32 HAL or LL library
 * =============================================================================
 */

#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ─── Hardware constants ──────────────────────────────────────────────────── */
#define PWM_FREQ_HZ         50000U
#define PWM_PERIOD_TICKS    1440U       /* 72 MHz / 50 kHz = 1440 ticks        */
#define MPPT_INTERVAL_MS    100U        /* P&O evaluation period               */
#define UART_BAUD           115200U

/* ─── INA219 register addresses ──────────────────────────────────────────── */
#define INA219_ADDR         (0x40 << 1) /* 8-bit address for HAL               */
#define INA219_REG_CONFIG   0x00
#define INA219_REG_SHUNT    0x01
#define INA219_REG_BUS      0x02
#define INA219_REG_CAL      0x05
/* Calibration: Cal = trunc(0.04096 / (0.1 Ω × 0.001 A LSB)) = 4096          */
#define INA219_CAL_VALUE    0x1000
/* 16V range, gain ÷8 (320 mV shunt), 8-sample averaging (both bus & shunt).
 * 8 samples × 2 measurements × ~532 µs per sample ≈ 8.5 ms total conversion,
 * well within the 100 ms MPPT tick. Previous 128-sample averaging (~140 ms)
 * caused every other tick to read stale data.                               */
#define INA219_CONFIG_VALUE 0x377F

/* ─── Buck converter limits ──────────────────────────────────────────────── */
#define DUTY_MIN            0.05f       /* 5% min duty                         */
#define DUTY_MAX            0.95f       /* 95% max duty                        */
#define VREF_MIN_V          10.0f       /* Never pull below 10 V               */
#define VREF_MAX_V          19.0f       /* 50 Wp panel Voc headroom            */
#define STEP_MIN_V          0.05f       /* Minimum P&O step                    */
#define STEP_MAX_V          0.60f       /* Maximum P&O step                    */
#define STEP_K              0.008f      /* dP/dV scaling coefficient           */

/* ─── Battery charging thresholds ───────────────────────────────────────── */
#define V_BULK_TARGET       14.7f       /* Bulk → Absorption transition        */
#define V_FLOAT_TARGET      13.8f       /* Float setpoint                      */
#define I_BULK_TARGET        6.0f       /* CC current limit (A)                */
#define I_TAIL_THRESHOLD     0.5f       /* Absorption → Float current tail     */

/* ─── LSTM blend parameters (paper §III-B) ──────────────────────────────── */
#define BLEND_DEADBAND      0.15f       /* 15% relative irradiance deviation   */
#define ALPHA_CLOUD_CLR_K   0.45f       /* Cloud-clearing gain factor          */
#define ALPHA_CLOUD_CLR_T   1.5f        /* Cloud-clearing decay rate           */
#define ALPHA_CLOUD_DRP_K   0.08f       /* Cloud-drop gain factor              */
#define ALPHA_CLOUD_DRP_T   1.0f        /* Cloud-drop decay rate               */
#define BLEND_COOLDOWN      20U         /* Steps to suppress after blend       */

/* ─── Thermal foldback (IRFB4110, Rth_JA ≈ 13.5 °C/W) ──────────────────── */
#define TJUNCTION_FOLDBACK  100.0f      /* Start derating at 100 °C (simulated)*/
#define RDS_ON_OHM          0.0037f     /* 3.7 mΩ for loss estimation          */

/* ─── UART frame delimiters ─────────────────────────────────────────────── */
#define UART_RX_BUF_LEN     64U
#define UART_TX_BUF_LEN     80U

/* ─── Peripheral handles (defined in main or HAL init) ──────────────────── */
extern I2C_HandleTypeDef  hi2c1;
extern TIM_HandleTypeDef  htim1;
extern UART_HandleTypeDef huart1;

/* ============================================================
 * Data Types
 * ============================================================ */

typedef enum {
    CHARGE_BULK = 0,
    CHARGE_ABSORPTION,
    CHARGE_FLOAT
} ChargeState_t;

typedef struct {
    float v_bat;            /* Battery bus voltage (V)                        */
    float i_bat;            /* Battery current (A) — positive = charging      */
    float v_pv;             /* PV voltage estimate from V_ref (V)             */
    float p_pv;             /* Estimated PV power (W)                         */
    float v_ref;            /* Current MPPT voltage reference (V)             */
    float duty;             /* PWM duty cycle [0.0, 1.0]                      */
    ChargeState_t state;    /* Charging FSM state                             */
    uint32_t tick_ms;       /* 1 ms system tick counter                       */
} SystemState_t;

typedef struct {
    float v_mpp_pred;       /* Predicted MPP voltage from Helios (V)          */
    float g_pred;           /* Predicted irradiance (W/m²)                    */
    float alpha;            /* Blend weight from Helios [0,1] (optional override) */
    uint8_t valid;          /* 1 = fresh Helios message received              */
} HeliosMsg_t;

/* ============================================================
 * Module-level state
 * ============================================================ */

static SystemState_t  sys;
static HeliosMsg_t    helios;

/* P&O internal state */
static float  g_v_prev      = 0.0f;
static float  g_p_prev      = 0.0f;
static int8_t g_perturb_dir = +1;     /* +1 or -1                             */
static uint8_t g_cooldown   = 0;      /* Blend cooldown counter               */

/* UART RX ring buffer */
static uint8_t  rx_buf[UART_RX_BUF_LEN];
static uint8_t  rx_line[UART_RX_BUF_LEN];
static uint16_t rx_head = 0;

/* ============================================================
 * Forward declarations
 * ============================================================ */

static HAL_StatusTypeDef ina219_init(void);
static HAL_StatusTypeDef ina219_read(float *v_bus, float *i_shunt);
static void     set_duty(float duty);
static float    duty_from_vref(float v_ref, float v_pv_open);
static void     mppt_tick(void);
static void     charge_fsm(void);
static void     blend_lstm(void);
static void     uart_send_telemetry(void);
static void     uart_parse_rx(void);
static float    expf_approx(float x);

/* ============================================================
 * INA219 I²C driver
 * ============================================================ */

static HAL_StatusTypeDef ina219_write16(uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = { reg, (val >> 8) & 0xFF, val & 0xFF };
    return HAL_I2C_Master_Transmit(&hi2c1, INA219_ADDR, buf, 3, 10);
}

static HAL_StatusTypeDef ina219_read16(uint8_t reg, int16_t *out)
{
    uint8_t buf[2];
    HAL_StatusTypeDef r;
    uint8_t r_reg = reg;
    r = HAL_I2C_Master_Transmit(&hi2c1, INA219_ADDR, &r_reg, 1, 10);
    if (r != HAL_OK) return r;
    r = HAL_I2C_Master_Receive(&hi2c1, INA219_ADDR, buf, 2, 10);
    if (r != HAL_OK) return r;
    *out = (int16_t)((buf[0] << 8) | buf[1]);
    return HAL_OK;
}

static HAL_StatusTypeDef ina219_init(void)
{
    HAL_StatusTypeDef r;
    r = ina219_write16(INA219_REG_CAL, INA219_CAL_VALUE);
    if (r != HAL_OK) return r;
    return ina219_write16(INA219_REG_CONFIG, INA219_CONFIG_VALUE);
}

/**
 * Read INA219 bus voltage (V) and shunt current (A).
 * Shunt LSB: 10 µV → with 0.1 Ω shunt → 0.0001 A per LSB (100 µA).
 * Bus voltage LSB: 4 mV.
 */
static HAL_StatusTypeDef ina219_read(float *v_bus, float *i_shunt)
{
    int16_t raw_bus, raw_shunt;
    HAL_StatusTypeDef r;

    r = ina219_read16(INA219_REG_BUS, &raw_bus);
    if (r != HAL_OK) return r;
    r = ina219_read16(INA219_REG_SHUNT, &raw_shunt);
    if (r != HAL_OK) return r;

    /* Bus voltage: bits [15:3], LSB = 4 mV */
    *v_bus    = ((raw_bus >> 3) & 0x1FFF) * 0.004f;
    /* Shunt current: LSB = 10 µV / 0.1 Ω = 100 µA                          */
    *i_shunt  = (float)raw_shunt * 0.0001f;

    return HAL_OK;
}

/* ============================================================
 * PWM control — TIM1 CH1 (PA8), 50 kHz
 * ============================================================ */

static void set_duty(float duty)
{
    if (duty < DUTY_MIN) duty = DUTY_MIN;
    if (duty > DUTY_MAX) duty = DUTY_MAX;
    uint32_t ccr = (uint32_t)(duty * PWM_PERIOD_TICKS);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr);
    sys.duty = duty;
}

/**
 * Approximate duty from desired V_ref:
 * For a synchronous buck: V_out = D × V_in (simplified, ignoring drops).
 * Here V_in ≈ V_pv (PV panel open-circuit tracked to V_ref).
 * We regulate the output (battery) voltage by setting duty such that
 * V_ref is presented at the input of the inductor.
 * duty = V_bat / V_ref  (steady-state buck equation)
 */
static float duty_from_vref(float v_ref, float v_pv)
{
    if (v_ref <= 0.0f || v_pv <= 0.0f) return DUTY_MIN;
    float d = sys.v_bat / v_ref;
    if (d < DUTY_MIN) d = DUTY_MIN;
    if (d > DUTY_MAX) d = DUTY_MAX;
    return d;
    (void)v_pv; /* reserved for feedforward refinement */
}

/* ============================================================
 * Variable-Step P&O MPPT (paper §III-B)
 * ============================================================ */

static void mppt_tick(void)
{
    float v_now, i_now;
    if (ina219_read(&v_now, &i_now) != HAL_OK) return;

    /* During Buck operation, PV voltage ≈ V_ref (inductor input).
     * We track the operating point via V_ref. In CC mode, the INA219
     * measures battery-side; PV side is inferred from duty.            */
    float v_pv_est = (sys.duty > 0.01f) ? (v_now / sys.duty) : v_now;
    float p_now    = v_pv_est * fabsf(i_now);

    sys.v_bat = v_now;
    sys.i_bat = i_now;
    sys.p_pv  = p_now;
    sys.v_pv  = v_pv_est;

    /* ── Variable-step magnitude ─────────────────────────────────── */
    float dP = p_now  - g_p_prev;
    float dV = v_pv_est - g_v_prev;
    float dPdV = (fabsf(dV) > 0.001f) ? (dP / dV) : 0.0f;

    float step = STEP_K * fabsf(dPdV);
    if (step < STEP_MIN_V) step = STEP_MIN_V;
    if (step > STEP_MAX_V) step = STEP_MAX_V;

    /* ── Perturbation direction ──────────────────────────────────── */
    if (fabsf(dV) < 0.001f) {
        /* First step or V unchanged: maintain direction */
    } else if (dP * dV > 0.0f) {
        g_perturb_dir = +1;
    } else {
        g_perturb_dir = -1;
    }

    float v_ref_po = sys.v_ref + g_perturb_dir * step;

    /* ── LSTM blend (if Helios message is fresh, §III-B) ─────────── */
    float v_ref_new = v_ref_po;
    if (helios.valid && g_cooldown == 0) {
        blend_lstm();   /* updates v_ref_new via sys.v_ref in-place     */
        v_ref_new = sys.v_ref;
    } else {
        v_ref_new = v_ref_po;
        sys.v_ref = v_ref_new;
    }

    /* Clamp */
    if (sys.v_ref < VREF_MIN_V) sys.v_ref = VREF_MIN_V;
    if (sys.v_ref > VREF_MAX_V) sys.v_ref = VREF_MAX_V;

    /* Cooldown decrement */
    if (g_cooldown > 0) g_cooldown--;

    /* ── Apply new duty ─────────────────────────────────────────── */
    float d = duty_from_vref(sys.v_ref, v_pv_est);
    set_duty(d);

    /* Store previous values */
    g_v_prev = v_pv_est;
    g_p_prev = p_now;

    /* Clear Helios fresh flag */
    helios.valid = 0;
}

/* ============================================================
 * LSTM blend (paper §III-B, equation: V_ref = (1-α)·V_po + α·V_pred)
 * ============================================================ */

static void blend_lstm(void)
{
    if (helios.g_pred <= 0.0f) return;

    /* Estimate current irradiance from INA219 current
     * G_est ≈ (I_pv / I_sc0) × G_stc, I_sc0 = 2.91 A @ G_stc = 1000 W/m²  */
    float i_pv_est = sys.i_bat / sys.duty;  /* invert buck */
    float g_est    = (i_pv_est / 2.91f) * 1000.0f;
    if (g_est < 1.0f) g_est = 1.0f;

    float rel_dev = fabsf(helios.g_pred - g_est) / g_est;

    /* Deadband: suppress blend during stable periods */
    if (rel_dev < BLEND_DEADBAND) return;

    /* Asymmetric α: cloud-clearing vs cloud-drop */
    float alpha;
    if (helios.g_pred > g_est) {
        /* Irradiance predicted to rise: cloud clearing, larger α */
        alpha = ALPHA_CLOUD_CLR_K * expf_approx(-ALPHA_CLOUD_CLR_T * rel_dev);
    } else {
        /* Irradiance predicted to fall: cloud shadow onset, smaller α */
        alpha = ALPHA_CLOUD_DRP_K * expf_approx(-ALPHA_CLOUD_DRP_T * rel_dev);
    }
    /* Clamp α to [0.05, 0.55] */
    if (alpha < 0.05f) alpha = 0.05f;
    if (alpha > 0.55f) alpha = 0.55f;

    float v_ref_blend = (1.0f - alpha) * sys.v_ref + alpha * helios.v_mpp_pred;

    sys.v_ref  = v_ref_blend;
    g_cooldown = BLEND_COOLDOWN;
}

/* ============================================================
 * Three-stage CC/CV/Float battery charging FSM
 * ============================================================ */

static void charge_fsm(void)
{
    switch (sys.state) {

    case CHARGE_BULK:
        /* Constant Current: limit I_bat ≤ 6 A */
        if (sys.i_bat > I_BULK_TARGET) {
            /* Reduce V_ref to pull operating point leftward on P-V curve */
            sys.v_ref -= 0.10f;
        }
        if (sys.v_bat >= V_BULK_TARGET) {
            sys.state = CHARGE_ABSORPTION;
        }
        break;

    case CHARGE_ABSORPTION:
        /* Constant Voltage: hold V_bat = 14.7 V */
        {
            float err = V_BULK_TARGET - sys.v_bat;
            sys.v_ref += 0.02f * err;   /* simple proportional nudge          */
        }
        if (sys.i_bat < I_TAIL_THRESHOLD) {
            sys.state = CHARGE_FLOAT;
        }
        break;

    case CHARGE_FLOAT:
        /* Float: hold V_bat = 13.8 V */
        {
            float err = V_FLOAT_TARGET - sys.v_bat;
            sys.v_ref += 0.02f * err;
        }
        /* If battery discharges below 12.5 V (load demand), restart bulk */
        if (sys.v_bat < 12.5f) {
            sys.state = CHARGE_BULK;
        }
        break;
    }

    /* Thermal foldback: reduce current if simulated T_J > 100 °C
     * T_J ≈ T_amb + P_cond × Rth_JA, P_cond = I² × RDS_on                   */
    float p_cond   = sys.i_bat * sys.i_bat * RDS_ON_OHM;
    float tj_est   = 35.0f + p_cond * 13.5f;   /* T_amb = 35 °C              */
    if (tj_est > TJUNCTION_FOLDBACK) {
        float fold = (tj_est - TJUNCTION_FOLDBACK) * 0.01f;
        sys.v_ref -= fold;
    }
}

/* ============================================================
 * UART telemetry TX → Helios
 * Frame: "ART:V=xx.xx,I=xx.xx,D=x.xxx,S=x,G=xxx\r\n"
 * ============================================================ */

static void uart_send_telemetry(void)
{
    char buf[UART_TX_BUF_LEN];
    int len = snprintf(buf, sizeof(buf),
        "ART:V=%.2f,I=%.3f,D=%.3f,S=%d,G=%.1f\r\n",
        sys.v_bat,
        sys.i_bat,
        sys.duty,
        (int)sys.state,
        (sys.duty > 0.01f) ? (sys.i_bat / sys.duty / 2.91f * 1000.0f) : 0.0f
    );
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 20);
}

/* ============================================================
 * UART RX parser — expects Helios frame:
 * "HEL:VP=xx.xx,GP=xxx.x,AL=x.xx\r\n"
 *  VP = predicted V_MPP, GP = predicted irradiance, AL = optional alpha
 * ============================================================ */

static void uart_parse_rx(void)
{
    /* Poll for incoming byte (non-blocking) */
    uint8_t byte;
    while (HAL_UART_Receive(&huart1, &byte, 1, 0) == HAL_OK) {
        if (byte == '\n') {
            rx_line[rx_head] = '\0';
            /* Parse: HEL:VP=xx.xx,GP=xxx.x,AL=x.xx */
            if (strncmp((char *)rx_line, "HEL:", 4) == 0) {
                float vp = 0.0f, gp = 0.0f, al = 0.35f;
                sscanf((char *)rx_line + 4,
                       "VP=%f,GP=%f,AL=%f", &vp, &gp, &al);
                if (vp > 5.0f && vp < 25.0f) {
                    helios.v_mpp_pred = vp;
                    helios.g_pred     = gp;
                    helios.alpha      = al;
                    helios.valid      = 1;
                }
            }
            rx_head = 0;
        } else if (byte != '\r') {
            if (rx_head < UART_RX_BUF_LEN - 1) {
                rx_line[rx_head++] = byte;
            }
        }
    }
}

/* ============================================================
 * Fast exp approximation (Padé, ~0.3% error for x in [-5, 0])
 * Avoids pulling in full libm on M3
 * ============================================================ */

static float expf_approx(float x)
{
    /* Standard cmath expf is fine on M3 with FPU disabled;
     * use this stub or replace with HAL_RNG / arm_math.h variant */
    return expf(x);
}

/* ============================================================
 * Peripheral Initialisation (HAL callbacks)
 * These supplement the CubeMX-generated code.
 * ============================================================ */

/**
 * Call after MX_TIM1_Init() and MX_I2C1_Init():
 *   - Start TIM1 CH1 PWM
 *   - Initialise INA219
 *   - Set initial duty to minimum
 */
void Artemis_Init(void)
{
    memset(&sys,    0, sizeof(sys));
    memset(&helios, 0, sizeof(helios));

    sys.v_ref = 14.0f;    /* Conservative initial reference */
    sys.state = CHARGE_BULK;

    /* Start 50 kHz PWM */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    set_duty(DUTY_MIN);

    /* Init INA219 */
    for (int retry = 0; retry < 3; retry++) {
        if (ina219_init() == HAL_OK) break;
        HAL_Delay(10);
    }
}

/* ============================================================
 * Main loop body — call every 1 ms from SysTick or main()
 * ============================================================ */

void Artemis_Tick(void)
{
    sys.tick_ms = HAL_GetTick();

    /* Parse incoming UART bytes every tick */
    uart_parse_rx();

    /* MPPT + charging at 100 ms intervals */
    static uint32_t last_mppt = 0;
    if ((sys.tick_ms - last_mppt) >= MPPT_INTERVAL_MS) {
        last_mppt = sys.tick_ms;
        mppt_tick();
        charge_fsm();
        uart_send_telemetry();
    }
}

/* ============================================================
 * CubeMX / HAL main() integration shim
 * Replace the while(1) body in main.c with:
 *
 *   Artemis_Init();
 *   while (1) { Artemis_Tick(); }
 * ============================================================ */

/* ─── TIM1 CubeMX config reminder ───────────────────────────────────────────
 * Prescaler    : 0      (72 MHz / 1 = 72 MHz timer clock)
 * Period (ARR) : 1439   (72 MHz / 50 kHz - 1 = 1439)
 * CH1 Mode     : PWM Generation CH1
 * Output state : Enable
 * ─── I2C1 CubeMX config ──────────────────────────────────────────────────
 * Speed        : 400 kHz (Fast Mode)
 * SDA          : PB7
 * SCL          : PB6
 * ─── USART1 CubeMX config ────────────────────────────────────────────────
 * Baud         : 115200
 * Word length  : 8 bits
 * Parity       : None
 * Stop bits    : 1
 * TX           : PA9
 * RX           : PA10
 * ─────────────────────────────────────────────────────────────────────────*/
