# SCG-ECU - CONFIGURAÇÃO BMW 330i E46

**Veículo:** BMW 330i E46 (M54B30 3.0L)
**ECU:** SCG-ECU 2.0 STM32F407VGT6
**Firmware:** Speeduino
**Software:** TunerStudio MS
**Data:** 2025-11-07

---

## 📋 PRÉ-REQUISITOS

Antes de iniciar a configuração, verifique:

### Hardware:
- ✅ SCG-ECU instalado e conectado ao chicote
- ✅ Sensor MAP instalado (GM 3-bar)
- ✅ TPS mecânico instalado
- ✅ Wideband O2 (LSU 4.9) instalado
- ✅ CAN-Bus conectado (pins PA11/PA12)
- ✅ Todos os 6 injetores conectados
- ✅ Todas as 6 bobinas conectadas
- ✅ VANOS solenoids + position sensors conectados

### Software:
- ✅ TunerStudio MS instalado (versão 3.1.10+)
- ✅ SCG-ECU firmware atualizado (Speeduino latest)
- ✅ Driver USB/Serial instalado
- ✅ Arquivo `.ini` correto para SCG-ECU

---

## 🚀 PASSO 1: CRIAR NOVO PROJETO

### 1.1 - Criar Base Tune

1. Abrir TunerStudio MS
2. **File → New Project**
3. Nome: `BMW_330i_E46_M54B30`
4. Controller: `Speeduino STM32F407`
5. Comunicação: `USB/Serial (115200 baud)`

### 1.2 - Configurar Comunicação

```
Settings → Communications
- Port:          Selecionar porta COM da SCG-ECU
- Baud Rate:     115200
- Data Bits:     8
- Stop Bits:     1
- Parity:        None
```

**Teste de Conexão:**
- Clicar em "Test Port"
- Deve aparecer: "Communication OK"
- Se falhar: verificar drivers USB/Serial

---

## ⚙️ PASSO 2: ENGINE CONSTANTS

### 2.1 - Basic Settings

```
Settings → Engine Constants

Engine Type:             4-Stroke Even Fire
Number of Cylinders:     6
Engine Displacement:     2979 cc (2.98 L)
```

### 2.2 - Cylinder Configuration

```
Cylinder 1:              Enabled
Cylinder 2:              Enabled
Cylinder 3:              Enabled
Cylinder 4:              Enabled
Cylinder 5:              Enabled
Cylinder 6:              Enabled

Cylinder Arrangement:    Inline
Firing Order:            1-5-3-6-2-4
```

---

## 🔧 PASSO 3: TRIGGER SETUP (CRÍTICO!)

### 3.1 - Primary Trigger (Crankshaft)

```
Settings → Trigger Setup → Primary Trigger

Trigger Pattern:         Missing Tooth
Number of Teeth:         60
Missing Teeth:           2
Trigger Angle:           90° (BTDC cyl #1)

Trigger Edge:            Rising
Trigger Filter:          Medium (50 µs)
```

**⚠️ IMPORTANTE:**
- Missing teeth posicionados a 90° BTDC do cilindro #1
- Este é o padrão BMW M54B30 (60-2 wheel)

### 3.2 - Secondary Trigger (Camshaft)

```
Settings → Trigger Setup → Secondary Trigger

Use Secondary Trigger:   Yes
Trigger Type:            Camshaft Position Sensor
Number of Pulses:        1 per cycle
Trigger Edge:            Rising

Sync Method:             CMP-based (sequential)
```

**Função:**
- Determinar qual cilindro está em combustion stroke
- Necessário para injeção/ignição sequencial

### 3.3 - Trigger Angle Calibration

**Procedimento:**
1. Girar motor manualmente até TDC cilindro #1 (compression stroke)
2. Verificar sinal de trigger no TunerStudio (Tools → Trigger Wizard)
3. Ajustar "Trigger Angle" até leitura mostrar 0° (BTDC)
4. Travar valor

**Valores Esperados:**
```
Trigger Angle:           90° BTDC
```

---

## 💉 PASSO 4: FUEL SETUP

### 4.1 - Injection Configuration

```
Settings → Injection → Injection Configuration

Injection Mode:          Sequential
Injection Timing:        Port Injection
Injector Staging:        Normal (no staging)

Number of Squirts:       1 per cycle
Injector Layout:         Sequential (6 outputs)
```

**Sequential Timing (720° cycle):**
```
Cylinder 1:  0°
Cylinder 5:  120°
Cylinder 3:  240°
Cylinder 6:  360°
Cylinder 2:  480°
Cylinder 4:  600°
```

### 4.2 - Injector Characteristics

```
Settings → Injection → Injector Characteristics

Injector Flow Rate:      236 cc/min @ 3 bar (OEM)
                         (or 282 cc/min static)
Fuel Pressure:           3.5 bar (51 PSI)

Injector Dead Time:
- @ 14V:                 0.8 ms
- @ 13V:                 1.0 ms
- @ 12V:                 1.2 ms
- @ 11V:                 1.4 ms
- @ 10V:                 1.8 ms

Injector Opening Time:   0.8 ms (nominal)
Injector Closing Time:   0.5 ms
```

**⚠️ Nota:** Se usar injetores upgrade (440cc, 600cc), ajustar flow rate!

### 4.3 - Fuel Pump Control

```
Settings → Outputs → Fuel Pump

Fuel Pump Output:        Enabled
Output Pin:              Auxiliary Output 1
Prime Time:              3 seconds
Prime Pulse:             100% duty

Trigger Condition:       Engine Running (RPM > 0)
Cutoff Delay:            2 seconds after engine stops
```

---

## 🔥 PASSO 5: IGNITION SETUP

### 5.1 - Ignition Configuration

```
Settings → Ignition → Ignition Configuration

Ignition Mode:           Sequential Coil-on-Plug
Number of Coils:         6
Coil Arrangement:        Sequential (6 outputs)

Firing Order:            1-5-3-6-2-4
```

### 5.2 - Coil Characteristics

```
Settings → Ignition → Coil Characteristics

Coil Type:               Bosch Pencil Coil (0 221 504 470)
Primary Resistance:      0.8 Ω
Secondary Resistance:    1.0 kΩ (old) / 1.8 kΩ (new)

Dwell Time (Fixed):
- @ 14V:                 3.2 ms
- @ 13V:                 3.5 ms
- @ 12V:                 4.0 ms
- @ 11V:                 4.5 ms

Max Dwell:               6.0 ms (safety limit)
```

**⚠️ IMPORTANTE:**
- Dwell muito alto = sobrecarga das bobinas (aquecimento)
- Dwell muito baixo = ignição fraca (falha de combustão)

### 5.3 - Spark Advance Base

```
Settings → Ignition → Spark Advance

Timing Method:           Table-based (3D)
Base Timing:             10° BTDC
Maximum Advance:         40° BTDC
Maximum Retard:          5° ATDC (limp mode)

Fixed Timing (test):     15° BTDC (para calibração inicial)
```

---

## 🌡️ PASSO 6: SENSORS CONFIGURATION

### 6.1 - CLT (Coolant Temperature)

```
Settings → Sensors → CLT

Sensor Type:             NTC Thermistor (BMW OEM)
Pull-up Resistor:        2200 Ω
Reference Voltage:       5.0V

Calibration Table:
  Temp (°C)  |  Resistance (Ω)
  ----------|-----------------
    -40     |     100,000
    -20     |      25,000
      0     |       9,500
     20     |       2,500
     40     |       1,150
     80     |         300
    100     |         180
    120     |         105
```

**Teste:**
- Motor frio: ~20°C (deve ler ~2500Ω)
- Motor quente: ~86°C (deve ler ~270Ω)

### 6.2 - IAT (Intake Air Temperature)

```
Settings → Sensors → IAT

Sensor Type:             NTC Thermistor (BMW OEM)
Pull-up Resistor:        2200 Ω
Reference Voltage:       5.0V

Calibration Table:       (idêntico ao CLT)
```

### 6.3 - TPS (Throttle Position Sensor)

```
Settings → Sensors → TPS

Sensor Type:             Potentiometer (0-5V)
Minimum Voltage:         0.5V (throttle closed)
Maximum Voltage:         4.5V (WOT)

Calibration:
- Closed Throttle:       0.5V → 0% TPS
- Wide Open Throttle:    4.5V → 100% TPS

Filter:                  Light (10 Hz cutoff)
```

**Procedimento de Calibração:**
1. Motor desligado
2. Tools → Calibrate TPS
3. Fechar borboleta totalmente → clicar "Set Closed"
4. Abrir borboleta totalmente → clicar "Set Open"
5. Salvar

### 6.4 - MAP (Manifold Absolute Pressure)

```
Settings → Sensors → MAP

Sensor Type:             GM 3-Bar MAP
Pull-up Resistor:        None (sensor tem output voltage)
Reference Voltage:       5.0V

Calibration Table:
  Pressure (kPa)  |  Voltage (V)
  ---------------|-------------
        0        |      0.5
       50        |      1.17
      100        |      2.00
      150        |      2.83
      200        |      3.50
      250        |      4.17
      300        |      4.5

Filter:                  Medium (20 Hz cutoff)
```

**Valores de Referência:**
```
Key Off:                 ~100 kPa (atmospheric, varia com altitude)
Idle (hot):              35-45 kPa (vacuum)
Cruising:                50-70 kPa
WOT:                     95-105 kPa (naturally aspirated)
```

### 6.5 - O2 Sensor (Wideband)

```
Settings → Sensors → O2 Sensor

Sensor Type:             LSU 4.9 Wideband
Controller:              SCG-ECU Built-in
Reference Voltage:       5.0V

Lambda Range:            0.65 - 1.30
AFR Range:               9.5 - 19.0 (gasoline)

Calibration:
- Free Air Calibration:  Run at key-on (antes de dar partida)
- Target Lambda:         0.90 (WOT), 1.00 (cruise), 1.05 (light load)

Closed-Loop Control:     Enabled (após aquecimento)
Heater Control:          Automatic (PWM)
```

### 6.6 - Knock Sensors

```
Settings → Sensors → Knock Sensors

Number of Sensors:       2
Sensor Type:             Piezoelectric (BMW OEM)

Sensor 1 Position:       Between cylinders 2-3
Sensor 2 Position:       Between cylinders 4-5

Frequency Range:         5-15 kHz (center ~8 kHz)
Threshold:               30% (initial, ajustar após logging)

Knock Response:
- Retard Amount:         3° per event
- Maximum Retard:        10° total
- Recovery Rate:         1° per second
```

---

## 🌀 PASSO 7: VVT SETUP (VANOS)

### 7.1 - VVT Configuration

```
Settings → VVT Control

VVT Mode:                Closed-Loop (PID control)
Number of VVT:           2 (Dual - Intake + Exhaust)

VVT1 (Intake):
- Control Pin:           PC6 (TIM3_CH1)
- Position Sensor:       PA5 (ADC1_IN5)
- PWM Frequency:         200 Hz

VVT2 (Exhaust):
- Control Pin:           PC7 (TIM3_CH2)
- Position Sensor:       PA6 (ADC1_IN6)
- PWM Frequency:         200 Hz
```

### 7.2 - Position Sensor Calibration

```
VVT1 Position Sensor (Intake):
- Min Voltage:           0.5V  →  0° advance
- Max Voltage:           4.5V  →  40° advance
- Filter:                Light (10 Hz)

VVT2 Position Sensor (Exhaust):
- Min Voltage:           0.5V  →  0° advance
- Max Voltage:           4.5V  →  20° advance
- Filter:                Light (10 Hz)
```

### 7.3 - PID Controller Settings

```
VVT1 PID (Intake):
- P Gain:                1.5
- I Gain:                0.3
- D Gain:                0.05
- Output Limit:          0-100% duty

VVT2 PID (Exhaust):
- P Gain:                1.5
- I Gain:                0.3
- D Gain:                0.05
- Output Limit:          0-100% duty
```

### 7.4 - Target Timing Tables

**VVT1 Intake Timing Table (base conservative):**
```
         MAP (kPa) →
RPM ↓    30    50    70    90    100
-------------------------------------
1000     0     5    10    15     18
2000     5    10    15    20     23
3000    10    15    20    25     28
4000    15    20    25    30     33
5000    20    25    30    35     38
6000    15    20    25    30     33

Valores em graus (camshaft degrees)
```

**VVT2 Exhaust Timing Table (base conservative):**
```
         MAP (kPa) →
RPM ↓    30    50    70    90    100
-------------------------------------
1000     0     0     2     5      8
2000     0     2     5     8     10
3000     2     5     8    10     12
4000     5     8    10    12     15
5000     8    10    12    15     18
6000     5     8    10    12     15

Valores em graus (camshaft degrees)
```

---

## 📊 PASSO 8: BASE TABLES

### 8.1 - VE Table (Volumetric Efficiency)

**Base Map M54B30 (naturally aspirated):**
```
         MAP (kPa) →
RPM ↓    30    50    70    90    100
-------------------------------------
1000    40    50    60    70     75
1500    42    52    62    72     77
2000    45    55    65    75     80
2500    48    58    68    78     83
3000    50    60    70    80     85
3500    52    62    72    82     87
4000    54    64    74    84     89
4500    55    65    75    85     90
5000    56    66    76    86     91
5500    54    64    74    84     89
6000    52    62    72    82     87
6500    48    58    68    78     83

Valores em % (volumetric efficiency)
```

**⚠️ Nota:** Esta é uma base inicial! Ajustar via autotune ou dyno.

### 8.2 - Spark Advance Table

**Base Map M54B30 (pump gas 95 RON):**
```
         MAP (kPa) →
RPM ↓    30    50    70    90    100
-------------------------------------
1000    15    12    10     8      6
1500    18    15    12    10      8
2000    22    18    15    12     10
2500    26    22    18    15     12
3000    30    26    22    18     15
3500    32    28    24    20     17
4000    34    30    26    22     19
4500    35    31    27    23     20
5000    36    32    28    24     21
5500    34    30    26    22     19
6000    32    28    24    20     17
6500    30    26    22    18     15

Valores em graus BTDC
```

**⚠️ CRÍTICO:**
- Esta tabela é conservadora (evitar knock)
- Ajustar com knock sensor feedback
- NUNCA exceder MBT (maximum brake torque timing)

### 8.3 - AFR Target Table

**Base Map (stoich cruise, rich WOT):**
```
         MAP (kPa) →
RPM ↓    30    50    70    90    100
-------------------------------------
1000   15.0  14.7  14.5  14.0   13.5
1500   15.0  14.7  14.5  14.0   13.2
2000   15.2  14.7  14.3  13.8   13.0
2500   15.2  14.7  14.3  13.5   12.8
3000   15.2  14.7  14.0  13.2   12.5
3500   15.0  14.7  13.8  13.0   12.3
4000   15.0  14.7  13.5  12.8   12.0
4500   14.7  14.5  13.2  12.5   12.0
5000   14.7  14.3  13.0  12.3   11.8
5500   14.7  14.0  12.8  12.0   11.8
6000   14.7  13.8  12.5  12.0   11.8

Valores em AFR (Air-Fuel Ratio)
Lambda = AFR / 14.7
```

**Lógica:**
- Light load (30-50 kPa): Lean (15.0-15.2 AFR) para economia
- Mid load (70 kPa): Stoich (14.7 AFR)
- High load (90-100 kPa): Rich (12.0-13.5 AFR) para potência/proteção

---

## 📡 PASSO 9: CAN-BUS SETUP

### 9.1 - CAN Configuration

```
Settings → CAN-Bus → Configuration

CAN-Bus Protocol:        BMW DME (MS43)
CAN Speed:               500 kbps
CAN Termination:         Enabled (120Ω)

Hardware Pins:
- CAN-H:                 PA11 (CAN1_RX)
- CAN-L:                 PA12 (CAN1_TX)
```

### 9.2 - CAN Messages to Transmit

```
Enable BMW DME Messages:

DME1 (0x316):            Enabled (RPM + Torque)
- Frequency:             10 ms (100 Hz)
- Priority:              High

DME2 (0x329):            Enabled (Temperature + TPS)
- Frequency:             100 ms (10 Hz)
- Priority:              Medium

DME4 (0x545):            Enabled (Warning lights)
- Frequency:             200 ms (5 Hz)
- Priority:              Low
```

**Resultado:**
- Painel de instrumentos mostra RPM
- Temperatura do motor exibida
- MIL (Check Engine Light) funcional

### 9.3 - OBD-II Support

```
Settings → OBD-II

OBD-II Protocol:         ISO 15765-4 (CAN)
Mode Support:
- Mode 01:               Enabled (live data)
- Mode 03:               Enabled (read DTCs)
- Mode 04:               Enabled (clear DTCs)
- Mode 09:               Enabled (vehicle info)

PIDs Supported:
- 0x0C:                  Engine RPM
- 0x0D:                  Vehicle Speed
- 0x05:                  Coolant Temperature
- 0x0F:                  Intake Air Temperature
- 0x11:                  Throttle Position
- 0x33:                  Barometric Pressure
```

---

## 🛡️ PASSO 10: ENGINE PROTECTION

### 10.1 - Rev Limiter

```
Settings → Limits → Rev Limiter

Hard Rev Limit:          6750 RPM (fuel + ignition cut)
Soft Rev Limit:          6500 RPM (ignition retard -10°)

Cut Method:              Fuel + Ignition cut (alternating cylinders)
Hysteresis:              100 RPM
```

**⚠️ Nota:** M54B30 OEM redline = 6500 RPM, fuel cut @ 6750 RPM

### 10.2 - Temperature Protection

```
Settings → Limits → Temperature Protection

CLT Warning:             105°C (fan high speed)
CLT Critical:            115°C (power reduction 50%)
CLT Emergency:           120°C (engine shutdown)

IAT Warning:             55°C (retard timing -3°)
IAT Critical:            65°C (retard timing -6°, enrich AFR)
```

### 10.3 - Knock Protection

```
Settings → Knock Control

Knock Detection:         Enabled (both sensors)
Sensitivity:             Medium (threshold 30%)

Knock Response:
- Initial Retard:        3° per knock event
- Max Retard:            10° total
- Recovery Rate:         1° per second
- AFR Enrichment:        5% richer on knock detected
```

---

## 🧪 PASSO 11: INITIAL STARTUP

### 11.1 - Pre-Start Checklist

```
☐ Verificar todas as conexões elétricas
☐ Verificar pressão de óleo (gauge mecânico)
☐ Verificar nível de combustível (> 1/4 tank)
☐ Desconectar bobinas (cranking test primeiro!)
☐ TunerStudio conectado e logging
```

### 11.2 - Cranking Test (sem combustão)

**Objetivo:** Verificar trigger, sensores, sem risco de backfire

1. Desconectar todas as 6 bobinas (ou desabilitar ignição no TS)
2. Desabilitar bomba de combustível
3. Cranking por 5 segundos
4. Verificar no TunerStudio:
   - **RPM:** Deve mostrar ~200-300 RPM (cranking speed)
   - **Sync:** Deve mostrar "Synced" (trigger OK)
   - **CLT:** Temperatura ambiente
   - **MAP:** ~35-45 kPa (vacuum durante cranking)
   - **TPS:** ~0-2% (throttle closed)

**Se RPM = 0:**
- Verificar trigger wheel (CKP sensor)
- Verificar wiring PA15 (CKP input)

### 11.3 - First Start

**Procedimento:**
1. Reconectar bobinas
2. Habilitar ignição e bomba
3. Prime fuel pump (3 segundos)
4. Cranking (não mais que 10 segundos)
5. Motor deve pegar e idle (rough inicial é normal)

**Se não pegar:**
- Verificar injetores (teste de clique)
- Verificar ignição (spark test)
- Verificar timing (ajustar trigger angle)

### 11.4 - Idle Tuning

**Valores iniciais:**
```
Idle Target RPM:         750 RPM (cold), 700 RPM (hot)
Idle Control:            Closed-loop (PID)
IAC Type:                Stepper motor (se houver) ou PWM valve

PID Tuning:
- P:                     2.0 (start)
- I:                     0.5
- D:                     0.1
```

**Ajuste Manual:**
- Motor quente em idle
- Ajustar "Idle Target RPM" para ~700-750 RPM
- Ajustar "Idle VE" (VE table @ idle cells) para AFR alvo (14.7)

---

## 📈 PASSO 12: ROAD TUNING

### 12.1 - Data Logging

**Canais para logar:**
```
- RPM
- MAP
- TPS
- AFR (wideband)
- Spark Advance
- VVT1 Position (intake)
- VVT2 Position (exhaust)
- CLT
- IAT
- Knock Count
```

### 12.2 - VE Table Autotune

**Método:**
1. Ativar "Auto-tune VE" no TunerStudio
2. Dirigir normalmente (variar RPM e load)
3. Software ajusta VE table automaticamente para atingir AFR target
4. Rodar por ~30-60 minutos
5. Desativar auto-tune e salvar tabela

### 12.3 - Spark Advance Tuning

**⚠️ REQUER DYNO ou muito cuidado!**

**Método MBT (Maximum Brake Torque):**
1. Fixar load e RPM (ex: 3000 RPM, 70 kPa)
2. Avançar timing +2°
3. Medir torque (dyno) ou aceleração (datalog)
4. Repetir até torque parar de aumentar
5. Retardar -1° para margem de segurança
6. Salvar valor

**⚠️ Se detectar knock:** Retardar imediatamente!

### 12.4 - VANOS Optimization

1. Baseline: VANOS desligado (0° ambos)
2. Testar avanço intake: +10°, +20°, +30°, +40°
3. Medir torque em cada ponto
4. Repetir para diferentes RPM/load
5. Preencher tabela com melhores valores

---

## 🔧 TROUBLESHOOTING

### Motor não pega:

```
Sintoma                  | Possível Causa              | Solução
------------------------|-----------------------------|-----------------------
RPM = 0 no TS           | CKP sensor não lê           | Verificar wiring PA15
RPM OK, mas não pega    | Sem injeção                 | Testar injetores
RPM OK, injeção OK      | Sem ignição                 | Testar bobinas
Pega e morre            | Trigger angle errado        | Ajustar timing base
```

### Idle instável:

```
Oscila ±100 RPM         | VANOS seals gastas          | Rebuild VANOS
Idle muito baixo        | Leak de vácuo               | Verificar mangueiras
Idle muito alto         | TPS não calibrado           | Recalibrar TPS
```

### CAN-Bus não funciona:

```
Painel não mostra RPM   | CAN-Bus desabilitado        | Ativar BMW DME protocol
CAN funciona, mas erros | Baud rate errado            | Verificar 500 kbps
Nenhum dado CAN         | Terminação 120Ω faltando    | Adicionar resistor
```

---

## 🎯 PRÓXIMOS PASSOS

Após configuração inicial:

1. ✅ **Dyno Tune:** Otimizar VE, Spark, VANOS tables
2. ✅ **Data Logging:** Monitorar knock, AFR, VVT performance
3. ✅ **Emissions Test:** Verificar lambda, catalisadores (se aplicável)
4. ✅ **Long-term Testing:** Dirigir por 500+ km, verificar reliability
5. ✅ **Backup Tune:** Salvar `.msq` file em múltiplas localizações!

---

## 🔗 RECURSOS

### TunerStudio:
- **Download:** https://www.tunerstudio.com/index.php/downloads
- **Manual:** https://www.tunerstudio.com/index.php/manuals

### Speeduino:
- **Wiki:** https://wiki.speeduino.com/
- **Forum:** https://speeduino.com/forum/

---

**Última atualização:** 2025-11-07
**Versão:** 1.0
**Status:** ✅ PRONTO PARA IMPLEMENTAÇÃO
