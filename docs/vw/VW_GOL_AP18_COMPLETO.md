# VW GOL QUADRADO AP 1.8 - DOCUMENTAÇÃO COMPLETA SCG-ECU 2.0
## CONFIGURAÇÃO PARA MOTOR VOLKSWAGEN AP 1.8 MI (1994)

**Versão Documento:** 1.0
**Data:** 01/11/2025
**Projeto:** SCG-ECU 2.0 - STM32F407VGT6 8x8
**Aplicação:** Volkswagen Gol Quadrado AP 1.8 MI (1994)
**Status:** DOCUMENTAÇÃO TÉCNICA COMPLETA

---

## ÍNDICE

1. [Visão Geral do Projeto](#1-visão-geral-do-projeto)
2. [Especificações do Motor AP 1.8](#2-especificações-do-motor-ap-18)
3. [Sistema de Injeção Original](#3-sistema-de-injeção-original)
4. [Configuração SCG-ECU - Versão Aspirada](#4-configuração-scg-ecu---versão-aspirada)
5. [Configuração SCG-ECU - Versão Turbo](#5-configuração-scg-ecu---versão-turbo)
6. [Sensores e Calibração](#6-sensores-e-calibração)
7. [Tabelas de Mapeamento](#7-tabelas-de-mapeamento)
8. [Pinout e Fiação](#8-pinout-e-fiação)
9. [Roadmap de Implementação](#9-roadmap-de-implementação)
10. [Recursos SCG-ECU Disponíveis](#10-recursos-scg-ecu-disponíveis)

---

## 1. VISÃO GERAL DO PROJETO

### 1.1 Objetivo

Implementar a **SCG-ECU 2.0** (Speeduino STM32F407 refatorado) no **Volkswagen Gol Quadrado AP 1.8 MI (1994)**, substituindo o sistema original **FIC EEC-IV monoponto** por um sistema moderno de gerenciamento eletrônico.

### 1.2 Fases do Projeto

**FASE 1: ASPIRADO ORIGINAL (ATUAL)**
- Motor AP 1.8 original (sem modificações)
- Configuração conservadora e confiável
- Injeção multiponto sequencial (upgrade do monoponto)
- Ignição mapeada com distribuidor Hall
- Objetivo: Melhorar resposta, consumo e confiabilidade

**FASE 2: TURBO (FUTURA)**
- Turbocompressor Garrett GT2860RS ou similar
- Boost: 0.5 a 1.0 bar (7-15 psi)
- Injetores high-flow
- Ignição totalmente mapeada
- Controle de boost integrado
- Objetivo: 150-180 cv de forma confiável

### 1.3 Hardware Alvo

**SCG-ECU 2.0:**
- MCU: STM32F407VGT6 @ 168 MHz
- Flash: 1MB / RAM: 192KB
- 8 canais de injeção independentes
- 8 canais de ignição independentes
- Suporta: Turbo, VVT, Launch Control, Flat Shift, etc.

**Comparação com Original:**

| Aspecto | FIC EEC-IV (Original) | SCG-ECU 2.0 |
|---------|----------------------|-------------|
| CPU | 8-bit (~8 MHz) | 32-bit ARM @ 168 MHz |
| Injetores | 1 (monoponto) | 8 independentes |
| Ignição | 1 saída (distribuidor) | 8 saídas (wasted/sequential) |
| Resolução timing | ~1° | 0.1° |
| Mapeamento | Fechado (chip) | Totalmente editável |
| Boost control | Não | Sim (PWM) |
| Launch control | Não | Sim |
| Flat shift | Não | Sim |
| Flex fuel | Não | Sim (com sensor) |
| Data logging | Não | SD card + Serial |

---

## 2. ESPECIFICAÇÕES DO MOTOR AP 1.8

### 2.1 Dados Técnicos Fundamentais

```
Motor:              Volkswagen AP 1.8 (Alta Performance)
Código:             AP 1800
Ano:                1994 (Gol Quadrado)
Tipo:               4 cilindros em linha, 8 válvulas
Arquitetura:        Longitudinal, refrigerado a água
Bloco:              Ferro fundido
Cabeçote:           Alumínio, SOHC (comando no cabeçote)
```

### 2.2 Dimensões

```
Cilindrada:         1781 cm³ (1.8 litros)
Diâmetro (bore):    81.0 mm
Curso (stroke):     86.4 mm
Relação bore/stroke: 0.938 (undersquare - torque)
Comprimento biela:  144 mm
Relação R/L:        0.300 (rod-to-stroke ratio)
```

### 2.3 Taxa de Compressão

```
Carburado:
  - Gasolina:       8.5:1
  - Álcool:         12.3:1

Injeção MI (1994):
  - Gasolina:       10.0:1
  - Álcool:         13.0:1

Turbo (futuro):
  - Gasolina:       8.5:1 (rebaixado)
  - Álcool:         9.5:1 (rebaixado)
```

### 2.4 Performance Original

**Versão Aspirada MI (1994):**
```
Potência máxima:    97 cv @ 5,250 RPM (gasolina)
Torque máximo:      15.5 kgfm (152 Nm) @ 3,000 RPM
RPM idle:           850 ± 50 RPM
RPM redline:        6,000 RPM (conservador)
RPM máximo:         6,500 RPM (hard limit)
```

**Performance Esperada Turbo (Fase 2):**
```
Potência máxima:    150-180 cv @ 5,500 RPM (0.8 bar boost)
Torque máximo:      24-28 kgfm @ 3,500 RPM
Boost:              0.5-1.0 bar (7-15 psi)
RPM redline:        6,500 RPM
RPM máximo:         7,000 RPM
```

### 2.5 Características do Motor

**Ordem de Ignição:**
```
1 - 3 - 4 - 2

Cilindro #1: Lado da polia/correia dentada
Cilindro #4: Lado do volante
```

**Sincronismo de Válvulas:**
```
Comando:            SOHC (Single OverHead Camshaft)
Acionamento:        Correia dentada
Válvulas/cilindro:  2 (1 admissão + 1 escape)
Folga válvulas:
  - Admissão:       0.15-0.25 mm (frio)
  - Escape:         0.30-0.40 mm (frio)
```

**Sistema de Arrefecimento:**
```
Tipo:               Refrigeração líquida
Capacidade:         ~6.5 litros
Termostato:         87°C (abre)
Temperatura normal: 85-95°C
Temperatura máxima: 105°C (warning)
```

**Lubrificação:**
```
Pressão mínima:     1.0 bar @ idle
Pressão normal:     3.0-4.5 bar @ 3000 RPM
Óleo recomendado:   15W40 (mineral) / 5W30 (sintético)
Capacidade:         3.5 litros (com filtro)
```

---

## 3. SISTEMA DE INJEÇÃO ORIGINAL

### 3.1 FIC EEC-IV CFI (1994)

**Características:**
```
Tipo:               Monoponto (TBI - Throttle Body Injection)
Fabricante:         FIC (Ford/VW)
Sistema:            EEC-IV CFI (Central Fuel Injection)
ECU:                8-bit microcontroller
Injetores:          1x monoponto
Ignição:            Distribuidor Hall + módulo
Lambda:             Narrowband (0.1-0.9V)
```

**Injetor Monoponto:**
```
Localização:        Corpo de borboleta
Resistência:        2.0 ± 0.5 Ω (low impedance)
Vazão:              ~150-180 cc/min @ 1 bar
Pressão:            1.0 bar (regulador no TBI)
Tempo injeção:      ~2-15 ms (típico)
```

**Sistema de Ignição:**
```
Tipo:               Distribuidor mecânico
Sensor:             Hall effect (3 fios)
Janelas:            4 (3 iguais + 1 maior)
Bobina:             Simples (1 saída)
Controle dwell:     ECU (fixo ~3ms)
Avanço:             100% eletrônico (sem avanço mecânico)
Avanço inicial:     9° BTDC @ idle
Avanço total:       22° BTDC @ WOT high RPM
```

### 3.2 Sensores Originais

**TPS (Throttle Position Sensor):**
```
Tipo:               Potenciômetro resistivo
Alimentação:        5V
Sinal:              0-5V (linear)
Closed throttle:    ~0.5V (10%)
WOT:                ~4.5V (90%)
Resistência:        ~5kΩ
```

**MAP (Manifold Absolute Pressure):**
```
Tipo:               Sensor de frequência
Alimentação:        5V ± 5%
Sinal:              80.9-162.4 Hz
Pressão mínima:     ~20 kPa (80.9 Hz)
Pressão máxima:     ~100 kPa (162.4 Hz)
Idle típico:        ~35 kPa (~95 Hz)
```

**CLT (Coolant Temperature):**
```
Tipo:               NTC (Negative Temperature Coefficient)
Montagem:           M10x10 (19mm)
Resistência:
  - 0°C:            ~6000 Ω
  - 25°C:           ~2500 Ω
  - 80°C:           ~300 Ω
  - 100°C:          ~180 Ω
Pullup resistor:    2490 Ω
```

**IAT (Intake Air Temperature):**
```
Tipo:               NTC (igual CLT)
Localização:        Coletor de admissão
Curva:              Mesma do CLT
Range típico:       10-60°C
```

**Lambda/O2 Sensor:**
```
Tipo:               Zircônia narrowband (NÃO aquecido em 1994)
Fios:               1 fio (sinal)
Sinal:              0.1-0.9V
Lambda 1.0:         ~0.45V (450mV)
Lean:               0.1-0.3V
Rich:               0.6-0.9V
Tempo aquecimento:  ~60-90 segundos
```

**Sensor Hall (Distribuidor):**
```
Tipo:               Hall effect
Fios:               3 (5V, GND, Sinal)
Sinal:              0-5V (square wave)
Janelas:            4 por revolução (2 cil/rev)
RPM cálculo:        (freq × 60) / (janelas/2)
Sincronismo:        Janela maior = TDC cilindro 1
```

### 3.3 Atuadores Originais

**Atuador Marcha Lenta:**
```
Tipo:               Motor de passo (stepper motor)
Resistência:        50-80 Ω por bobina
Steps:              ~150-200 (range completo)
Controle:           PWM 100 Hz
Posição idle:       ~30-50 steps
```

**Bomba de Combustível:**
```
Tipo:               Elétrica submersível
Pressão:            1.0 bar (monoponto)
Vazão:              ~60 L/h
Controle:           Relé acionado por ECU
```

**Eletroventilador:**
```
Acionamento:        Termostato mecânico (87°C)
Controle ECU:       Não (original)
Upgrade SCG-ECU:    Sim (PWM control)
```

---

## 4. CONFIGURAÇÃO SCG-ECU - VERSÃO ASPIRADA

### 4.1 Visão Geral da Configuração

**Objetivo:** Motor AP 1.8 aspirado original com melhorias de desempenho, economia e confiabilidade.

**Principais Mudanças vs Original:**
1. ✅ Injeção **multiponto sequencial** (4 injetores)
2. ✅ Mapeamento 16x16 (vs tabelas fixas)
3. ✅ Lambda wideband (vs narrowband)
4. ✅ Controle preciso de ignição
5. ✅ Data logging completo
6. ✅ Proteções avançadas

### 4.2 Engine Configuration

```ini
[ENGINE_BASIC]
displacement = 1781          # cm³
cylinders = 4
stroke = 86.4               # mm
bore = 81.0                 # mm
rod_length = 144.0          # mm
compression_ratio = 10.0    # gasolina MI
fuel_type = GASOLINE        # ou ETHANOL

[ENGINE_GEOMETRY]
firing_order = 1-3-4-2
cylinder_1_position = FRONT_BELT_SIDE
```

### 4.3 Decoder Configuration

**Opção 1: Distribuidor Hall (Original 1994)**
```ini
[DECODER]
type = BASIC_DISTRIBUTOR
trigger_type = HALL
trigger_edge = RISING
cylinders_per_revolution = 2  # 4-stroke
trigger_teeth = 4             # 4 janelas Hall
trigger_angle = 0             # Sincronizado via janela maior
```

**Opção 2: Roda Fônica 60-2 (UPGRADE RECOMENDADO)**
```ini
[DECODER]
type = MISSING_TOOTH
primary_teeth = 60
missing_teeth = 2
trigger_edge = RISING
trigger_angle = 90            # 90° BTDC (ajustar conforme instalação)
secondary_trigger = NONE      # Sem sensor de fase (wasted spark)
# OU
secondary_trigger = CAM_HALL  # Com sensor fase (sequential)
```

**Vantagens Roda Fônica 60-2:**
- ✅ Resolução 6° (vs 90° do distribuidor)
- ✅ Sincronismo preciso em todas RPMs
- ✅ Permite sequential ignition (com sensor fase)
- ✅ Timing mais estável
- ✅ Preparado para turbo

**Custo Upgrade:** ~R$ 200-300 (roda + sensor + suporte)

### 4.4 Injection Configuration

```ini
[INJECTION]
mode = SEQUENTIAL           # Sequencial (requer sensor fase)
# OU
mode = SEMI_SEQUENTIAL      # Semi-seq (2 grupos, sem sensor fase)

num_injectors = 4
injector_layout = PAIRED    # Cil 1+4, 2+3

[INJECTORS]
flow_rate = 280             # cc/min @ 3 bar (24 lb/h)
impedance = HIGH            # >12Ω (ou LOW com resistores)
fuel_pressure = 3.0         # bar (regulador aftermarket)
dead_time = 1.0             # ms (ajustar conforme injetor)

# Exemplo: Bosch EV1 280cc
# - Flow: 280 cc/min @ 3 bar
# - Impedance: 12-16Ω (high-Z)
# - Dead time: ~1.0ms @ 14V

[INJECTION_TIMING]
mode = PORT_INJECTION
start_angle = 355           # 5° BTDC (início injeção)
# Timing otimizado para melhor atomização
```

### 4.5 Ignition Configuration

**Com Distribuidor (Original):**
```ini
[IGNITION]
mode = WASTED_SPARK_DISTRIBUTOR
num_outputs = 1
output_mode = GOING_HIGH    # Trigger bobina

[DWELL]
mode = VOLTAGE_COMPENSATED
base_dwell = 3.0            # ms @ 14V
voltage_correction = YES
dwell_table = [
    # Volts : Dwell (ms)
    10.0 : 4.0,
    12.0 : 3.5,
    14.0 : 3.0,
    16.0 : 2.8
]
```

**Sem Distribuidor (Upgrade - 4x Coil Packs):**
```ini
[IGNITION]
mode = WASTED_SPARK         # 2 bobinas (cil 1+4, 2+3)
# OU
mode = SEQUENTIAL           # 4 bobinas individuais (requer sensor fase)

num_outputs = 4
coil_layout = WASTED_14_23  # Cil 1+4 / 2+3

[COILS]
type = SMART_COIL           # Coil-on-plug com driver interno
charge_time = 3.5           # ms
dwell_limit = 6.0           # ms (proteção)
```

### 4.6 RPM Limits

```ini
[REV_LIMITER]
soft_limit = 6000           # RPM (warning)
hard_limit = 6500           # RPM (fuel cut)
limiter_type = FUEL_CUT     # Ou SPARK_CUT
hysteresis = 100            # RPM

launch_rpm = 4000           # RPM (se ativado)
flat_shift_rpm = 5500       # RPM (se ativado)
```

### 4.7 Fuel Corrections (Aspirado)

```ini
[FUEL_CORRECTIONS]
warmup_enrichment = YES     # WUE
afterstart_enrichment = YES # ASE
accel_enrichment = YES      # AE (pump shot)
decel_enleanment = YES      # DE
baro_correction = YES
iat_density_correction = YES
battery_voltage_correction = YES
closed_loop_lambda = YES    # Ajuste fino via O2

[WUE_WARMUP]
# Temp (°C) : Enrichment (%)
-10 : 180
0   : 150
20  : 130
40  : 115
60  : 105
80  : 100
100 : 100

[ASE_AFTERSTART]
duration = 5.0              # segundos
enrichment = 120            # % (20% extra)
taper = LINEAR

[ACCEL_ENRICHMENT]
mode = TPS_MAP_BLEND        # Híbrido TPS+MAP
tps_threshold = 5           # %/s
map_threshold = 10          # kPa/s
amount = 100                # % (dobrar PW)
decay_time = 1.0            # segundo
```

### 4.8 Ignition Advance (Aspirado - Gasolina)

```ini
[IGNITION_ADVANCE]
mode = 3D_TABLE             # RPM x MAP

# Base timing (conservador para comum)
idle_advance = 9            # graus @ 850 RPM
cruise_advance = 18-22      # graus @ cruise
wot_advance = 22-26         # graus @ WOT

# Limites de segurança
min_advance = 0             # graus
max_advance = 35            # graus (segurança)

[ADVANCE_CORRECTIONS]
clt_correction = YES        # -5° motor frio
iat_correction = YES        # -2° ar quente
knock_correction = YES      # -3° por evento (se tiver sensor)
idle_correction = YES       # +2° estabilizar idle
```

**Tabela Base de Avanço (16x16):**

| RPM \ MAP | 20kPa | 30kPa | 40kPa | 50kPa | 60kPa | 70kPa | 80kPa | 90kPa | 100kPa |
|-----------|-------|-------|-------|-------|-------|-------|-------|-------|--------|
| 850       | 9°    | 9°    | 9°    | 9°    | 9°    | 9°    | 9°    | 9°    | 9°     |
| 1500      | 22°   | 20°   | 18°   | 16°   | 14°   | 12°   | 12°   | 12°   | 12°    |
| 2000      | 28°   | 26°   | 24°   | 22°   | 20°   | 18°   | 16°   | 14°   | 14°    |
| 3000      | 32°   | 30°   | 28°   | 26°   | 24°   | 22°   | 20°   | 18°   | 18°    |
| 4000      | 34°   | 32°   | 30°   | 28°   | 26°   | 24°   | 22°   | 20°   | 20°    |
| 5000      | 34°   | 32°   | 30°   | 28°   | 26°   | 24°   | 22°   | 22°   | 22°    |
| 6000      | 32°   | 30°   | 28°   | 26°   | 24°   | 24°   | 24°   | 24°   | 24°    |

**NOTA:** Valores conservadores para gasolina comum (87 octanas). Ajustar conforme combustível.

### 4.9 AFR Targets (Aspirado)

```ini
[AFR_TARGETS]
mode = WIDEBAND_LAMBDA      # Wideband O2 (LSU 4.9)

fuel = GASOLINE
stoich = 14.7               # AFR estequiométrico gasolina

# Tabela Lambda Target (16x16)
```

**Lambda Targets (Gasolina):**

| RPM \ TPS | 0%    | 20%   | 40%   | 60%   | 80%   | 100%  |
|-----------|-------|-------|-------|-------|-------|-------|
| Idle      | 1.00  | 1.00  | 0.95  | 0.90  | 0.85  | 0.85  |
| 2000      | 1.05  | 1.00  | 0.95  | 0.90  | 0.85  | 0.85  |
| 4000      | 1.05  | 1.00  | 0.95  | 0.90  | 0.85  | 0.85  |
| 6000      | 1.00  | 0.95  | 0.90  | 0.85  | 0.85  | 0.85  |

**Conversão AFR:**
- Lambda 1.00 = 14.7:1 (estequiométrico - economia)
- Lambda 1.05 = 15.4:1 (lean cruise - máxima economia)
- Lambda 0.85 = 12.5:1 (rich WOT - máxima potência)

**Lambda Targets (Etanol):**
- Stoich: 9.0:1
- Cruise: Lambda 1.05 (9.5:1)
- WOT: Lambda 0.80 (7.2:1)

### 4.10 Engine Protection (Aspirado)

```ini
[PROTECTION]
enable_all = YES

[CLT_PROTECTION]
warning_temp = 105          # °C
power_reduce_temp = 110     # °C (reduz 20% potência)
limp_mode_temp = 115        # °C (reduz 50%)
power_reduction = 20        # % por estágio

[OVERRUN_FUEL_CUT]
enable = YES
activate_rpm = 1800         # RPM
deactivate_rpm = 1500       # RPM (histerese)
activate_tps = 2            # % (acelerador fechado)
min_clt = 60                # °C (só com motor quente)

[FLOOD_CLEAR]
enable = YES
trigger_tps = 95            # % (acelerador full)
trigger_rpm = 400           # RPM (cranking)
cut_fuel = 100              # % (sem injeção)
```

---

## 5. CONFIGURAÇÃO SCG-ECU - VERSÃO TURBO

### 5.1 Visão Geral Turbo

**FASE 2 - PREPARAÇÃO TURBO**

```
Objetivo:         150-225 cv confiáveis (depende boost)
Boost:            0.5-1.0 bar (7-15 psi)
Turbo:            Garrett GT2860RS (recomendado)
Wastegate:        Externa 38mm
Intercooler:      FMIC 450x300x76mm
Injetores:        4x 440cc high-flow
Bomba:            255 L/h high-pressure
Regulador FPR:    Ajustável 1:1 boost referenced
```

#### 5.1.1 Opções de Turbocompressor (Mercado Brasil)

**PESQUISA REAL - Turbos disponíveis Brasil:**

**KKK K-16 (Budget/Iniciante):**
```
Potência máxima:    Até 180cv
Boost ideal:        0.5-0.7 bar
Custo:              R$ 1,500-2,000 (usado/recondicionado)
Prós:
  ✓ Muito barato
  ✓ Popular preparações street
  ✓ Peças fáceis
Contras:
  ✗ Limitado ~5000 RPM (fica "pesado")
  ✗ Eficiência baixa
  ✗ Lag perceptível
Recomendação:       ⚠️ Setup inicial/aprendizado apenas
```

**Garrett GT2860R (Custo-Benefício):**
```
Potência máxima:    250-360cv (motor preparado)
Potência AP 1.8:    180-220cv @ 0.8-1.0 bar
Boost ideal:        1.0-1.5 bar
Custo:              R$ 3,500-4,500 (novo)
Prós:
  ✓ Confiável (bearing journal)
  ✓ Ampla faixa potência
  ✓ Suporte técnico
Contras:
  ✗ Spool moderado (journal bearing)
Recomendação:       ✅ OPÇÃO ECONÔMICA
```

**Garrett GT2860RS (RECOMENDADO):**
```
Potência máxima:    250-360cv
Potência AP 1.8:    180-225cv @ 0.8-1.0 bar
Boost ideal:        1.0-1.5 bar
Custo:              R$ 4,000-5,000 (novo)
Prós:
  ✓ Ball bearing (spool RÁPIDO)
  ✓ Menos lag que GT2860R
  ✓ Eficiência alta
  ✓ Confiável até 1.2 bar daily
Contras:
  ✗ Preço ~R$ 500 mais caro que "R"
Recomendação:       ✅ MELHOR CUSTO-BENEFÍCIO GERAL
```

**Garrett GT2871R (High Power):**
```
Potência máxima:    270-475cv
Potência AP 1.8:    220-280cv @ 1.0-1.5 bar
Boost ideal:        1.2-1.8 bar
Custo:              R$ 5,000-6,500 (novo)
Prós:
  ✓ Máxima potência
  ✓ Flow alto
  ✓ Durabilidade race
Contras:
  ✗ Lag maior (maior A/R)
  ✗ Resposta <3000 RPM fraca
  ✗ Motor forjado obrigatório
Recomendação:       ⚠️ Só se meta >250cv
```

**Turbinas Genéricas .42/.48/.50:**
```
Potência:           Varia (80-200cv)
Custo:              R$ 800-1,500
Prós:
  ✓ Muito barato
Contras:
  ✗ Qualidade inconsistente
  ✗ Sem dados técnicos confiáveis
  ✗ Vida útil curta
  ✗ Pode falhar catastroficamente
Recomendação:       ❌ NÃO RECOMENDADO (risco > economia)
```

#### 5.1.2 Potências Reais Atingíveis

**PESQUISA REAL - Setups comprovados AP 1.8 Brasil:**

**0.5 bar (7 psi) - CONSERVADOR:**
```
Potência:           150 cv @ 5,500 RPM
Torque:             20 kgfm @ 3,200 RPM
Setup:              Street daily confiável
Combustível:        Gasolina comum
Motor:              Original (10:1 CR) OK curto prazo
                    8.5:1 recomendado longo prazo
Injetores:          280cc OK (duty ~75%)
Turbo:              KKK K-16 ou GT2860R
Confiabilidade:     Alta (stress moderado)
```

**0.8 bar (11 psi) - STREET/PISTA:**
```
Potência:           180 cv @ 5,500 RPM
Torque:             24 kgfm @ 3,500 RPM
Setup:              Street com pista ocasional
Combustível:        Gasolina premium (95+ octanas)
Motor:              8.5:1 CR OBRIGATÓRIO
Injetores:          440cc OBRIGATÓRIO
Turbo:              GT2860RS (spool melhor)
Oil catch can:      MUITO recomendado
Confiabilidade:     Média-Alta (manutenção rigorosa)
```

**1.0 bar (14 psi) - RACE:**
```
Potência:           200-225 cv @ 5,500-6,000 RPM
Torque:             26-28 kgfm @ 3,800 RPM
Setup:              Pista/race (não daily ideal)
Combustível:        Gasolina premium OBRIGATÓRIA
Motor:              Forjado RECOMENDADO
                    8.0:1 CR ideal
Injetores:          440cc mínimo
Turbo:              GT2860RS ou GT2871R
Oil catch can:      OBRIGATÓRIO
Comando turbo:      RECOMENDADO (SPA 268°)
Confiabilidade:     Média (desgaste acelerado)
```

**>1.2 bar - RACE EXTREMO:**
```
Potência:           250-300+ cv
Setup:              Full race apenas
Motor:              Forjado OBRIGATÓRIO
                    Bielas SPA Super A-Beam
                    Pistões SPA 83mm 8.0:1
Confiabilidade:     Baixa (rebuild frequente)
Recomendação:       Fora escopo deste documento
```

### 5.2 Modificações Necessárias Motor

**CRÍTICO - Rebaixar Compressão:**
```
Original:         10.0:1 (gasolina MI)
Turbo (seguro):   8.5:1 (gasolina) / 9.5:1 (etanol)
Turbo (race):     8.0:1 (gasolina) / 8.5:1 (etanol)
```

#### 5.2.1 Métodos Rebaixar Compressão (Opções Reais Brasil)

**MÉTODO 1: Junta Cabeçote +1.5mm MLS**
```
Resultado:          ~8.5:1 CR
Custo:              R$ 800-1,200
Componentes:
  - Junta MLS +1.5mm:        R$ 400-600
  - ARP studs M10x1.5:       R$ 400-600
Prós:
  ✓ Sem abrir motor (economia tempo)
  ✓ Reversível (trocar junta)
  ✓ Funciona até 0.8 bar confiável
Contras:
  ✗ Altura motor aumenta ligeiramente
  ✗ Pode afetar geometria válvulas
Recomendação:       ✅ Ideal turbo 0.5-0.8 bar
```

**MÉTODO 2: Pistão 1.6 Álcool em 1.8**
```
Resultado:          ~9.0:1 CR
Custo:              R$ 400-600 (pistões usados)
Componentes:
  - 4x Pistões AP 1.6 álcool (usados)
  - Anéis novos
Prós:
  ✓ Muito barato
  ✓ Bom para turbo street 0.5 bar
Contras:
  ✗ Pistões fundidos (não forjados)
  ✗ Limitado ~0.8 bar máximo
  ✗ Não ideal >180cv
Recomendação:       ⚠️ Só turbo conservador
```

**MÉTODO 3: Pistões Forjados Côncavos (MELHOR OPÇÃO)**
```
Resultado:          8.0-8.5:1 CR (escolher)
Custo:              R$ 1,500-1,800 (kit SPA)
Ver seção 5.2.2 abaixo
Prós:
  ✓ Ideal turbo alta potência
  ✓ Até 400cv suportado
  ✓ Durabilidade alta
Contras:
  ✗ Caro
  ✗ Retífica obrigatória
Recomendação:       ✅ Turbo >0.8 bar ou >200cv
```

#### 5.2.2 Pistões Forjados (SPA Turbo Brasil)

**SPA Super A - 83mm (AP 1.8):**
```
Especificações:
  Diâmetro:         83.00mm (original AP 1.8)
  Oversize:         Disponível +0.25, +0.50, +1.00mm
  Compressão:       Côncavo (8.0:1, 8.5:1, 9.0:1)
  Material:         Alumínio forjado 2618-T6
  Revestimento:     Skirt coating (reduz fricção)
  Potência:         Até ~400cv real
  Anéis:            Total Seal inclusos (kit)

Kit completo (4x pistões):
  Inclui:           4x pistões + pinos + travas + anéis
  Preço:            R$ 1,500-1,800

Instalação:
  Retífica:         R$ 800-1,200 (honing + fit)
  Balanceamento:    R$ 300-500
```

**Alternativas Importadas (Comparação):**
```
JE Pistons 83mm:
  Preço:            R$ 2,500-3,200 (importado)
  Qualidade:        Excelente (USA)
  Lead time:        45-60 dias

Wiseco 83mm:
  Preço:            R$ 2,200-2,800
  Qualidade:        Muito boa (USA)
  Disponibilidade:  Melhor que JE

Recomendação:     SPA Turbo (nacional, custo/benefício)
```

#### 5.2.3 Bielas Forjadas (SPA Turbo Brasil)

**SPA Basic 144mm (Entrada):**
```
Especificações:
  Comprimento:      144mm (original AP 1.8)
  Material:         4340 forjado
  Parafusos:        3/8" (9.5mm)
  Potência:         Até 450cv
  Peso:             ~520g (balanceadas ±2g)

Preço:              R$ 1,200-1,500 (jogo 4x)

Uso:
  Turbo 0.5-0.8 bar: ✅ Excelente
  Turbo >0.8 bar:    ✅ OK (limite conservador)
```

**SPA Super A-Beam 144mm (Premium):**
```
Especificações:
  Comprimento:      144mm
  Material:         4340 forjado heat-treated
  Parafusos:        7/16" (11mm) ARP 2000
  Potência:         Até 600cv REAL
                    (marketing fala 1000cv - ignore)
  Peso:             ~540g (balanceadas ±1g)
  Design:           H-beam otimizado

Preço:              R$ 2,000-2,500 (jogo 4x)

Uso:
  Turbo 0.8-1.0 bar: ✅ Ideal
  Turbo >1.0 bar:    ✅ Excelente
  Race >250cv:       ✅ Recomendado
```

**Scat/Ancona Premium (Importadas):**
```
Preço:              R$ 3,000-4,000
Parafusos:          ARP 2000 ou L19
Potência:           Até 800cv
Recomendação:       Só se budget permite (overkill AP 1.8)
```

#### 5.2.4 Embreagem Cerâmica (Turbo)

**PROBLEMA:**
```
Motor turbo aumenta torque significativamente:
  Original:         15.5 kgfm @ 3000 RPM
  Turbo 0.5 bar:    20 kgfm (+29%)
  Turbo 0.8 bar:    24 kgfm (+55%)
  Turbo 1.0 bar:    28 kgfm (+81%)

Embreagem original: Aguenta ~16 kgfm máximo
Resultado: SLIP sob carga, aquecimento, falha
```

**SOLUÇÃO: Disco Cerâmico + Platô Reforçado**

**Disco Cerâmico 4 Pastilhas COM Molas (RECOMENDADO):**
```
Especificações:
  Tipo:             4 pads cerâmico com molas amortecimento
  Torque máx:       25 kgfm (200cv / 0.8 bar)
  Uso:              Daily driver + pista ocasional
  Pedal:            Suave (aceitável daily)
  Engagement:       Progressive (com molas)
  Vida útil:        40,000-60,000 km

Preço:              R$ 600-800

Prós:
  ✓ Confortável daily
  ✓ Suficiente 0.8 bar
  ✓ Durável
Contras:
  ✗ Limitado ~200cv
```

**Disco Cerâmico 6 Pastilhas SEM Molas (Race):**
```
Especificações:
  Tipo:             6 pads cerâmico SEM molas (puck style)
  Torque máx:       35 kgfm (280cv / 1.2 bar)
  Uso:              Track/race/arrancada
  Pedal:            ON/OFF (difícil daily)
  Engagement:       Abrupto (sem molas)
  Vida útil:        20,000-30,000 km

Preço:              R$ 800-1,200

Prós:
  ✓ Suporta alta potência
  ✓ Não slipa
Contras:
  ✗ Difícil trânsito (on/off)
  ✗ Desgaste sincronizadores câmbio
```

**Platô (Pressure Plate) - 900 lbs (RECOMENDADO):**
```
Força mola:         900 lbs (~408 kg)
Torque máx:         ~28 kgfm (240cv)
Pedal:              Médio (suportável daily)
Uso:                Street turbo até 0.8-1.0 bar

Preço:              R$ 400-600
```

**Platô 980 lbs (Race):**
```
Força mola:         980 lbs (~445 kg)
Torque máx:         ~32 kgfm (300cv)
Pedal:              Pesado (difícil daily)
Uso:                Race >250cv

Preço:              R$ 500-700
```

**SETUP POPULAR BRASIL (Custo-Benefício):**
```
✅ Disco cerâmico 4 pastilhas COM molas:  R$ 700
✅ Platô 900 lbs:                         R$ 500
✅ Rolamento:                             R$ 200
-------------------------------------------------
TOTAL:                                    R$ 1,400

Potência suportada:   Até 240cv (0.8-1.0 bar)
Uso:                  Daily + track ocasional
Marcas:               Displatec, Ceramic Power, ACT
```

**Reforços Motor (Atualizado):**
```
✅ OBRIGATÓRIOS TODOS SETUPS:
  - Junta cabeçote MLS +1.5mm:          R$ 500
  - Parafusos cabeçote ARP M10x1.5:     R$ 500
  - Retentores válvula (alta temp):     R$ 120
  - Embreagem cerâmica 4pad + 900lbs:   R$ 1,400

✅ TURBO >0.8 bar ADICIONAR:
  - Pistões forjados SPA 83mm 8.5:1:    R$ 1,700
  - Bielas forjadas SPA Basic 144mm:    R$ 1,400
  - Anéis Total Seal:                   Inclusos pistões
  - Bronzinas ACL Race:                 R$ 400
  - Retífica motor completa:            R$ 2,500

✅ TURBO >1.0 bar UPGRADE:
  - Bielas SPA Super A-Beam:            R$ 2,300
  - Embreagem 6 pads + 980lbs:          R$ 1,500
```

**Admissão/Escape:**
```
✅ Admissão:
  - Filtro cônico alto-fluxo
  - Duto 3" (76mm) até turbo
  - Intercooler frontal (FMIC)
  - Tubulação alumínio 2.5" (64mm)
  - BOV/Válvula alívio

✅ Escape:
  - Coletor turbo 4-1 (manifold)
  - Downpipe 3" com wastegate merge
  - Catalisador high-flow (ou test pipe)
  - Escape 2.5-3" até ponteira
```

### 5.3 Engine Configuration (Turbo)

```ini
[ENGINE_BASIC]
displacement = 1781
cylinders = 4
stroke = 86.4
bore = 81.0
rod_length = 144.0
compression_ratio = 8.5     # REBAIXADO para turbo
fuel_type = GASOLINE
aspiration = TURBOCHARGED   # *** NOVO ***

[TURBO_SETUP]
max_boost = 1.0             # bar (14.5 psi)
wastegate_type = EXTERNAL   # ou INTERNAL
boost_control = YES         # Controle ativo SCG-ECU
```

### 5.4 Decoder Configuration (Turbo)

**OBRIGATÓRIO: Roda Fônica 60-2**

```ini
[DECODER]
type = MISSING_TOOTH
primary_teeth = 60
missing_teeth = 2
trigger_edge = RISING
trigger_angle = 90          # Ajustar conforme instalação
secondary_trigger = CAM_HALL  # Sensor fase (sequential)

# Turbo exige sincronismo preciso!
```

**Sensor de Fase (Cam):**
```
Tipo:               Hall effect
Localização:        Comando de válvulas
Sinal:              1 pulso por rotação
Sincronismo:        Permite ignição/injeção sequential
```

### 5.5 Injection Configuration (Turbo)

```ini
[INJECTION]
mode = SEQUENTIAL           # OBRIGATÓRIO turbo
num_injectors = 4

[INJECTORS]
flow_rate = 440             # cc/min @ 3 bar (42 lb/h)
impedance = HIGH            # 12-16Ω
fuel_pressure = 3.0         # bar base
boost_reference = YES       # FPR 1:1 boost referenced
dead_time = 0.95            # ms (high-flow injetores)

# Exemplos injetores turbo:
# - Bosch EV14 440cc: 42 lb/h @ 3 bar
# - Siemens Deka 60lb: 630cc @ 3 bar
# - Escolher conforme potência alvo

[FUEL_SYSTEM]
pump_flow = 255             # L/h (Walbro GSS342)
regulator = BOOST_REFERENCED  # 1:1 (3 bar base + boost)
```

**Cálculo Injetores Turbo:**
```
Potência alvo:    180 cv
BSFC:             0.55 lb/hp/h (turbo)
Duty cycle max:   80%

Flow necessário = (180 × 0.55) / (4 × 0.80)
                = 99 / 3.2
                = 31 lb/h por injetor
                = ~320 cc/min

Margem segurança: 440cc (42 lb/h) ✅ OK
```

### 5.6 Ignition Configuration (Turbo)

**OBRIGATÓRIO: Ignição Individual (4x Coil Packs)**

```ini
[IGNITION]
mode = SEQUENTIAL           # 4 bobinas individuais
num_outputs = 4
coil_layout = INDIVIDUAL    # Cil 1, 2, 3, 4

[COILS]
type = SMART_COIL           # Coil-on-plug
charge_time = 4.0           # ms (maior para turbo)
dwell_limit = 7.0           # ms

# Exemplo: NGK U5015 ou Bosch 0221504461
```

### 5.7 RPM Limits (Turbo)

```ini
[REV_LIMITER]
soft_limit = 6500           # RPM
hard_limit = 7000           # RPM (fuel + spark cut)
limiter_type = COMBINED     # Fuel + Spark
hysteresis = 150            # RPM

launch_rpm = 4500           # RPM (2-step)
launch_retard = 15          # graus (build boost)

flat_shift_rpm = 6000       # RPM
flat_shift_cut_time = 150   # ms
```

### 5.8 Boost Control

**SCG-ECU tem controle boost integrado! ✅**

```ini
[BOOST_CONTROL]
enable = YES
mode = CLOSED_LOOP          # PID control

sensor_type = MAP           # Usar MAP sensor
target_source = 3D_TABLE    # RPM x TPS

[BOOST_TARGETS]
# RPM \ TPS
# Valores em kPa (100 kPa = atm, 150 kPa = 0.5 bar boost)
idle = 100                  # kPa (atmosférico)
cruise = 100-110            # kPa (0-0.1 bar)
part_throttle = 120-140     # kPa (0.2-0.4 bar)
wot_low_rpm = 140           # kPa (0.4 bar) @ 2000-3000 RPM
wot_high_rpm = 180-200      # kPa (0.8-1.0 bar) @ 4000+ RPM

[BOOST_CONTROL_VALVE]
type = 3_PORT_MAC           # MAC solenoid 3 vias
frequency = 30              # Hz (30-50 Hz típico)
duty_idle = 0               # % (wastegate aberta)
duty_max = 90               # % (wastegate fechada)

[BOOST_PID]
# Ajuste fino PID
kP = 2.5                    # Proporcional
kI = 0.8                    # Integral
kD = 0.1                    # Derivativo

slew_rate = 10              # kPa/s (ramp gradual)
```

**Hardware Boost Control:**
```
✅ Wastegate externa 38mm (ou interna turbo)
✅ Solenoid boost 3-port (MAC valve)
✅ Linha vácuo 4mm silicone
✅ Boost gauge mecânico/elétrico (backup)
✅ Válvula alívio (BOV/Blow-off)
```

### 5.9 Fuel Corrections (Turbo)

**Mesmas do aspirado + Boost enrichment:**

```ini
[BOOST_ENRICHMENT]
enable = YES
mode = PROGRESSIVE

# Boost (bar) : Enrichment (%)
0.0  : 0        # Atmosférico = sem enriquecimento
0.2  : 5        # +5%
0.4  : 10       # +10%
0.6  : 15       # +15%
0.8  : 20       # +20%
1.0  : 25       # +25%

# Proteção detonação
max_boost_enrichment = 30   # %
```

### 5.10 Ignition Advance (Turbo - Gasolina)

**CRÍTICO: Avanço MUITO mais conservador!**

```ini
[IGNITION_ADVANCE]
mode = 3D_TABLE_BOOST_RETARD

# Base timing turbo (gasolina premium 95+ octanas)
idle_advance = 12           # graus
cruise_advance = 18-22      # graus (atmosférico)
wot_na_advance = 20         # graus (sem boost)
wot_boost_advance = 12-16   # graus (com boost)

# BOOST RETARD (crítico!)
[BOOST_RETARD]
enable = YES
mode = PROGRESSIVE

# Boost (bar) : Retard (graus)
0.0  : 0        # Sem boost = timing normal
0.2  : -3       # -3° (slight boost)
0.4  : -6       # -6°
0.6  : -8       # -8°
0.8  : -10      # -10°
1.0  : -12      # -12° (timing final ~12-14° WOT)

max_boost_retard = 15       # graus (segurança)
```

**Tabela Avanço Turbo (16x16) - CONSERVADORA:**

| RPM \ MAP | 100kPa | 120kPa | 140kPa | 160kPa | 180kPa | 200kPa |
|-----------|--------|--------|--------|--------|--------|--------|
| 850       | 12°    | 12°    | 12°    | 12°    | 12°    | 12°    |
| 2000      | 18°    | 16°    | 14°    | 12°    | 10°    | 10°    |
| 3000      | 20°    | 18°    | 16°    | 14°    | 12°    | 12°    |
| 4000      | 22°    | 20°    | 18°    | 16°    | 14°    | 12°    |
| 5000      | 22°    | 20°    | 18°    | 16°    | 14°    | 12°    |
| 6000      | 20°    | 18°    | 16°    | 14°    | 12°    | 12°    |

**NOTA:**
- MAP >100 kPa = em boost
- Valores MUITO conservadores para gasolina comum
- Gasolina premium (>95 octanas): +2-3° possível
- Etanol: +4-6° possível (melhor resistência detonação)
- **SEMPRE** fazer dyno tuning final!

### 5.11 AFR Targets (Turbo)

**CRÍTICO: AFR mais rico em boost!**

```ini
[AFR_TARGETS]
mode = WIDEBAND_LAMBDA
fuel = GASOLINE
stoich = 14.7
```

**Lambda Targets Turbo (Gasolina):**

| RPM \ MAP | 100kPa | 120kPa | 140kPa | 160kPa | 180kPa | 200kPa |
|-----------|--------|--------|--------|--------|--------|--------|
| Idle      | 1.00   | 1.00   | 0.95   | 0.90   | 0.85   | 0.82   |
| 2000      | 1.05   | 1.00   | 0.95   | 0.90   | 0.85   | 0.82   |
| 4000      | 1.00   | 0.95   | 0.90   | 0.87   | 0.85   | 0.82   |
| 6000      | 0.95   | 0.90   | 0.87   | 0.85   | 0.82   | 0.80   |

**Conversão (boost):**
- Lambda 0.82 = 12.0:1 (rico - máxima potência + segurança)
- Lambda 0.85 = 12.5:1 (rico - potência)
- Lambda 0.90 = 13.2:1 (ligeiramente rico)

**IMPORTANTE:**
- ⚠️ NUNCA lambda > 0.90 em boost alto!
- ⚠️ EGT (Exhaust Gas Temp) monitorar: <900°C seguro
- ✅ Adicionar sensor EGT por cilindro (recomendado)

### 5.12 Knock Control (Turbo)

**CRÍTICO para Turbo!**

```ini
[KNOCK_CONTROL]
enable = YES
sensor_type = ANALOG        # Sensor piezo
sensor_count = 1            # Ou 4 (ideal = 1 por cilindro)

[KNOCK_DETECTION]
frequency_min = 5000        # Hz
frequency_max = 15000       # Hz (AP 1.8 ~7-9 kHz típico)
threshold = 2.5             # V (ajustar conforme ruído)
window_angle = 40           # graus após TDC

[KNOCK_RESPONSE]
retard_per_event = -3       # graus por knock detectado
max_retard = -12            # graus total
recovery_rate = 0.5         # graus/segundo (retornar avanço)
enrichment = +10            # % fuel (resfriar)

[KNOCK_PROTECTION]
continuous_knock_limit = 5  # eventos consecutivos
action = BOOST_CUT_50       # Reduz boost 50%
restore_time = 10           # segundos
```

**Hardware Knock:**
```
✅ 1-4x Sensores knock Bosch 0 261 231 006
✅ Montagem: Bloco motor (entre cilindros)
✅ Torque: 20 Nm
✅ Cabo blindado até ECU
```

### 5.13 Engine Protection (Turbo)

**Proteções extras para turbo:**

```ini
[PROTECTION]
enable_all = YES

[CLT_PROTECTION]
warning_temp = 100          # °C (turbo roda mais quente)
power_reduce_temp = 105     # °C
limp_mode_temp = 110        # °C
boost_cut_temp = 108        # °C (corta boost)

[OIL_PROTECTION]
enable = YES                # *** NOVO - CRÍTICO TURBO ***
min_pressure_idle = 1.0     # bar @ idle
min_pressure_cruise = 3.0   # bar @ cruise
warning_pressure = 2.0      # bar
limp_mode_pressure = 1.5    # bar

[EGT_PROTECTION]
enable = YES                # *** NOVO - CRÍTICO TURBO ***
max_egt = 900               # °C por cilindro
warning_egt = 850           # °C
action_egt = ENRICHMENT_20  # +20% fuel resfriar

[BOOST_CUT_PROTECTION]
max_boost = 1.1             # bar (hard limit)
overboost_cut = YES
overboost_time = 2.0        # segundos acima limite
```

### 5.14 Launch Control (Turbo)

**SCG-ECU tem launch control integrado! ✅**

```ini
[LAUNCH_CONTROL]
enable = YES
activation = CLUTCH_SWITCH  # Ou TPS + RPM

[LAUNCH_SETTINGS]
target_rpm = 4500           # RPM (build boost)
rpm_tolerance = 100         # RPM
retard_angle = 15           # graus (anti-lag effect)
fuel_cut = SOFT             # Soft cut (misfire)
spark_cut = ALTERNATING     # 50% cilindros

boost_target = 0.6          # bar (spool turbo parado)
max_time = 10               # segundos (segurança)

[LAUNCH_RELEASE]
release_condition = CLUTCH_RELEASE
rev_hang_time = 500         # ms
smooth_transition = YES
```

### 5.15 Flat Shift (Turbo)

**SCG-ECU tem flat shift integrado! ✅**

```ini
[FLAT_SHIFT]
enable = YES
activation = CLUTCH_SWITCH

[FLAT_SHIFT_SETTINGS]
activate_rpm = 3000         # RPM mínimo
cut_time = 100              # ms (cortar ignição)
spark_cut = 100             # % (full cut)
fuel_cut = 0                # % (manter fuel)

boost_maintain = YES        # Manter boost durante shift
max_shift_time = 500        # ms (segurança)
```

### 5.16 SISTEMA AR CONDICIONADO - CONTROLE INTEGRADO

**SCG-ECU tem suporte completo AR CONDICIONADO! ✅**

#### 5.16.1 Visão Geral Controle AC

O sistema de ar condicionado pode ser totalmente controlado pela SCG-ECU, oferecendo vantagens significativas sobre controle mecânico original:

**RECURSOS DISPONÍVEIS:**
```
✅ Idle-up automático (previne motor morrer)
✅ Desligamento automático WOT (máxima potência)
✅ Proteção CLT (desliga se motor quente)
✅ Proteção RPM mínimo (estabilidade)
✅ Integração perfeita com tuning
✅ Data logging completo (TunerStudio)
```

**COMPATIBILIDADE:**
```
✓ Aspirado:         100% compatível ✅
✓ Turbo 0.5 bar:    Compatível com planejamento ✅
✓ Turbo 0.8 bar:    Requer modificações layout ⚠️
✓ Turbo >1.0 bar:   Difícil (escolher prioridade) ⚠️
```

#### 5.16.2 Configuração Hardware AC

**INPUTS (Sensores):**

```ini
[AC_INPUT]
ac_button_pin = PE4         # Botão AC painel
input_type = DIGITAL
polarity = ACTIVE_LOW       # Ground quando pressionado
series_pressure_switch = YES  # Pressostato em série
debounce_time = 50          # ms (filtro bounce)
```

**Fiação Input:**
```
BOTÃO AC PAINEL:
  Terminal 1: 12V (ignição switch)
  Terminal 2: → Pressostato alta pressão

PRESSOSTATO ALTA PRESSÃO:
  Entrada: ← Botão AC
  Saída: → SCG-ECU PE4 (AC input)

SCG-ECU PE4:
  Estado normal: Pull-up 5V (HIGH)
  AC acionado: Ground através botão+pressostato (LOW)
  Proteção: Resistor pull-up interno 10kΩ
```

**OUTPUTS (Atuadores):**

```ini
[AC_OUTPUT]
compressor_relay_pin = PE3  # Relé compressor clutch
output_type = DIGITAL
active_state = HIGH         # 12V para ativar relé
relay_rating = 30A          # Relé 30A obrigatório
flyback_diode = YES         # Proteção bobina relé
```

**Fiação Output:**
```
SCG-ECU PE3:
  → Relé pino 86 (bobina positivo)

RELÉ 30A:
  Pino 85: Ground chassis
  Pino 86: ← SCG-ECU PE3 (controle)
  Pino 30: Bateria 12V (fusível 30A)
  Pino 87: → Compressor clutch coil

COMPRESSOR CLUTCH:
  Positivo: ← Relé pino 87
  Negativo: → Ground chassis
  Consumo: ~4-6A @ engage
```

**COMPONENTES NECESSÁRIOS:**
```
✅ 1x Relé automotivo 30A (4 ou 5 pinos):  R$ 20-40
✅ 1x Pressostato alta pressão (13-15 bar): R$ 80-150
✅ Fiação reforçada 12AWG (4mm²):           R$ 50-100
✅ Fusível 30A inline:                      R$ 10-20
✅ Conectores impermeáveis:                 R$ 30-50
-------------------------------------------------
TOTAL COMPONENTES CONTROLE:                 R$ 190-360
```

#### 5.16.3 Configuração Software AC

**IDLE-UP PARAMETERS:**

```ini
[AC_IDLE_UP]
enable = YES
idle_increase = 100         # RPM (+100 RPM sobre base)
duty_cycle_add = 8          # % (atuador idle valve)
activation_delay = 300      # ms (suavizar transição)
deactivation_delay = 200    # ms
smooth_ramp = YES           # Rampa suave RPM
ramp_time = 500             # ms
```

**Funcionamento Idle-Up:**
```
Idle normal:        850 RPM
AC acionado:        850 → 950 RPM (gradual 500ms)
Carga compressor:   ~0.5-1.5 cv
Compensação:        +100 RPM + duty cycle +8%
Resultado:          Idle estável sem oscilação
```

**PROTEÇÕES AC:**

```ini
[AC_PROTECTION]
# Proteção RPM mínimo
min_rpm = 600               # RPM (desliga abaixo)
min_rpm_hyst = 100          # RPM hysteresis

# Proteção temperatura
max_clt = 105               # °C (motor quente)
max_clt_hyst = 3            # °C hysteresis

# Proteção WOT (Wide Open Throttle)
wot_disable = YES
wot_tps_threshold = 90      # % (full throttle)
wot_tps_re_enable = 80      # % (hysteresis)

# Proteção alta carga
high_load_disable = YES
high_load_map = 95          # kPa (turbo: boost)
high_load_re_enable = 90    # kPa (hysteresis)

# Proteção stall
stall_protection = YES
stall_rpm = 500             # RPM (motor morrendo)
stall_cut_time = 100        # ms (corta rápido)

# Proteção bateria
min_battery_voltage = 11.5  # V (bateria fraca)
```

#### 5.16.4 Funcionamento Detalhado AC

**SEQUÊNCIA ATIVAÇÃO:**
```
T=0ms:
  1. Motorista pressiona botão AC painel
  2. Pressostato verifica pressão sistema (OK)
  3. SCG-ECU detecta input PE4 → LOW

T=50ms:
  4. Debounce completo (filtro bounce)
  5. SCG-ECU verifica proteções:
     ✓ RPM >600? SIM
     ✓ CLT <105°C? SIM
     ✓ TPS <90%? SIM
     ✓ Battery >11.5V? SIM

T=100ms:
  6. Aumenta target idle: 850 → 950 RPM (início ramp)
  7. Aumenta duty cycle idle valve: +8%

T=300ms:
  8. Delay activation completo
  9. Idle stabilizado em 950 RPM

T=350ms:
  10. Ativa relé compressor (PE3 → HIGH)
  11. Compressor clutch engage
  12. Carga compressor aplicada

T=500ms:
  13. Idle final estável em 950 RPM
  14. Sistema AC operando normalmente
```

**SEQUÊNCIA DESATIVAÇÃO:**
```
T=0ms:
  1. Motorista desliga AC OU proteção ativa
  2. SCG-ECU corta relé compressor (PE3 → LOW)
  3. Compressor clutch disengage

T=200ms:
  4. Delay deactivation completo
  5. Reduz target idle: 950 → 850 RPM (ramp)
  6. Reduz duty cycle: -8%

T=700ms:
  7. Idle retorna normal 850 RPM
```

**PROTEÇÃO WOT (Wide Open Throttle):**
```
Situação: Motorista pisa fundo (TPS >90%)

Ação imediata:
  1. SCG-ECU detecta TPS >90%
  2. Desliga AC compressor (PE3 → LOW)
  3. Retorna máxima potência motor
  4. Mantém idle normal (não reduz ainda)

Quando TPS <80% (hysteresis):
  5. Re-ativa AC compressor
  6. Retorna idle-up
```

**PROTEÇÃO CLT (Coolant Temperature):**
```
Situação: Motor aquecendo excessivo (CLT >105°C)

Ação:
  1. SCG-ECU monitora CLT contínuo
  2. CLT atinge 105°C
  3. Desliga AC compressor (PE3 → LOW)
  4. Dashboard warning LED (se implementado)
  5. Mantém desligado até CLT <102°C (hysteresis)
```

#### 5.16.5 Compatibilidade Turbo + AC

**DESAFIO FÍSICO:**
```
Intercooler FMIC frontal (450x300mm) compete espaço com:
  - Radiador água (original 400x400mm)
  - Condensador AC (original 350x450mm)
  - Tubulação intercooler (2.5" alumínio)
  - Ar-flow radiador/condensador
```

**SOLUÇÕES COMPROVADAS:**

**OPÇÃO 1: Radiador Elétrico Compacto + Offset (RECOMENDADA)**
```
Modificação:
  - Remover radiador mecânico original grande
  - Instalar radiador elétrico 350x250mm
  - Montar offset lateral ou superior
  - Libera espaço frontal para FMIC + AC

Componentes:
  - Radiador alumínio elétrico:     R$ 500-800
  - 2x Fans elétricos 12" PWM:      R$ 300-500
  - Termostato eletrônico:          R$ 100-150

Vantagens:
  ✓ Mantém AC original completo
  ✓ FMIC 450x300mm full size
  ✓ Airflow otimizado (sem fan mecânica)
  ✓ SCG-ECU controla fans (PWM)

Desvantagens:
  ✗ Custo adicional R$ 900-1,450
  ✗ Consumo elétrico maior (alternador 60A+)

Recomendação: ✅ Turbo 0.5-0.8 bar + AC diário
Custo total:  ~R$ 1,200
```

**OPÇÃO 2: Compressor Elétrico 12V (MELHOR PERFORMANCE)**
```
Modificação:
  - Remover compressor mecânico (não rouba potência!)
  - Instalar compressor elétrico 12V automotivo
  - Montagem livre (não precisa polia motor)
  - Máxima liberdade routing

Componentes:
  - Compressor elétrico 15,000 BTU:  R$ 1,200-1,800
  - Controlador PWM:                 R$ 200-300
  - Upgrade alternador 70-90A:       R$ 400-700

Vantagens:
  ✓ Não rouba potência motor (~1-2 cv saving)
  ✓ Máxima liberdade instalação
  ✓ Compatível qualquer boost
  ✓ SCG-ECU controla PWM

Desvantagens:
  ✗ Custo elevado R$ 1,800-2,800
  ✗ Consumo elétrico alto (~60-80A peak)
  ✗ Alternador grande obrigatório

Recomendação: ✅ Turbo >0.8 bar + AC essencial
Custo total:  ~R$ 2,500
```

**OPÇÃO 3: FMIC Menor (COMPROMISSO)**
```
Modificação:
  - FMIC reduzido 350x250mm (ao invés 450x300mm)
  - Mantém AC original completo
  - Mantém radiador água original

Vantagens:
  ✓ Custo zero adicional
  ✓ AC original 100% preservado
  ✓ Instalação simples

Desvantagens:
  ✗ Intercooler menor = eficiência -20%
  ✗ Potência limitada ~160cv (0.6 bar máx)
  ✗ IAT pós-intercooler +10-15°C

Recomendação: ⚠️ Só turbo conservador 0.5 bar
                 OU se AC é prioridade absoluta
```

**OPÇÃO 4: Sem AC (MÁXIMA PERFORMANCE)**
```
Decisão:
  - Remover sistema AC completo
  - FMIC full size 450x300mm
  - Radiador água otimizado
  - Máxima potência turbo

Vantagens:
  ✓ Máxima performance
  ✓ Peso -15kg
  ✓ Sem complexidade

Desvantagens:
  ✗ Sem AC (óbvio)

Recomendação: ✅ Race/pista dedicado
                 Clima frio
```

**RECOMENDAÇÃO GERAL (Decisão):**
```
┌─────────────────────────────────────────────────────────┐
│ ASPIRADO:                                               │
│   → AC 100% compatível ✅                              │
│   → Custo: R$ 200 (controle apenas)                    │
│                                                          │
│ TURBO 0.5 bar (150cv):                                  │
│   → AC possível (planejamento) ✅                       │
│   → Opção 3 (FMIC menor) OU Opção 1 (radiador offset)  │
│   → Custo adicional: R$ 0-1,200                         │
│                                                          │
│ TURBO 0.8 bar (180cv):                                  │
│   → AC difícil mas viável ⚠️                            │
│   → Opção 1 (radiador offset) OBRIGATÓRIA              │
│   → Custo adicional: R$ 1,200                           │
│                                                          │
│ TURBO >1.0 bar (200cv+):                                │
│   → AC muito difícil ⚠️                                 │
│   → Opção 2 (compressor elétrico) OU sem AC            │
│   → Custo adicional: R$ 2,500 OU R$ 0                  │
│                                                          │
│ ESCOLHA: Performance OU Conforto?                       │
└─────────────────────────────────────────────────────────┘
```

#### 5.16.6 Pinout Completo AC

**DIAGRAMA ELÉTRICO COMPLETO:**
```
┌──────────────────────────────────────────────────────────────┐
│                    SISTEMA AC - SCG-ECU                      │
└──────────────────────────────────────────────────────────────┘

[BATERIA 12V] ───(Fusível 30A)─── [Relé pino 30]
                                         │
                                   [Relé pino 87]
                                         │
                                         └───> [Compressor Clutch (+)]
                                                      │
                                                  [Ground (−)]

[Ignição 12V] ────> [Botão AC painel]
                           │
                    [Pressostato 13bar]
                           │
                   [SCG-ECU PE4 INPUT]
                   (Pull-up 5V interno)

[SCG-ECU PE3 OUTPUT] ────> [Relé pino 86 (+)]
                                    │
                              [Relé pino 85 (−)]
                                    │
                                [Ground]
```

**TABELA PINOUT:**
```
┌────────────────────┬──────────────┬─────────────────────┐
│ Componente         │ Pino         │ Conexão             │
├────────────────────┼──────────────┼─────────────────────┤
│ SCG-ECU INPUT      │ PE4          │ AC request (botão)  │
│ SCG-ECU OUTPUT     │ PE3          │ Compressor relay    │
│ Botão AC           │ 1            │ 12V ignição         │
│ Botão AC           │ 2            │ → Pressostato       │
│ Pressostato        │ IN           │ ← Botão AC          │
│ Pressostato        │ OUT          │ → SCG-ECU PE4       │
│ Relé               │ 30           │ Bateria 12V (fused) │
│ Relé               │ 87           │ → Compressor +      │
│ Relé               │ 86           │ ← SCG-ECU PE3       │
│ Relé               │ 85           │ Ground              │
│ Compressor clutch  │ +            │ ← Relé 87           │
│ Compressor clutch  │ −            │ Ground chassis      │
└────────────────────┴──────────────┴─────────────────────┘
```

#### 5.16.7 Troubleshooting AC

**AC não liga (Compressor não engage):**
```
□ Verificar botão AC funcionando (multímetro)
□ Verificar pressostato (jump teste se suspeita)
□ Verificar input PE4 (deve ser LOW quando acionado)
□ Verificar RPM >600 (proteção mínima)
□ Verificar CLT <105°C (proteção temperatura)
□ Verificar TPS <90% (proteção WOT)
□ Verificar output PE3 (deve ter 12V quando ativo)
□ Verificar relé funcionando (trocar teste)
□ Verificar compressor clutch (alimentação chegando?)
□ Verificar ground compressor (resistência <1Ω)
□ TunerStudio: Dashboard AC status (debug)
```

**Idle oscila/cai com AC ligado:**
```
□ Aumentar idle_increase (+50 RPM)
    [AC_IDLE_UP] idle_increase = 150

□ Aumentar duty_cycle_add (+2-3%)
    [AC_IDLE_UP] duty_cycle_add = 11

□ Aumentar activation_delay (+100-200ms)
    [AC_IDLE_UP] activation_delay = 500

□ Verificar PID idle control (tune base primeiro)
□ Verificar vacuum leaks (piora com AC)
□ Verificar idle valve limpa (carbon buildup)
```

**AC desliga sozinho durante condução:**
```
□ Verificar proteção WOT (normal behavior!)
    → TPS >90% = desliga AC (max potência)
    → Voltar <80% = religa automático

□ Verificar CLT >105°C (proteção ativa)
    → Resolver overheat motor primeiro

□ Verificar pressostato alta pressão
    → Gás R134a sobrecarga (>15 bar)
    → Condensador bloqueado (airflow)

□ Verificar fiação PE4 intermitente
    → Re-crimpar conectores
    → Verificar vibração causando falso-contato

□ TunerStudio: Data logging AC events
```

**AC funciona mas esfriamento fraco:**
```
□ Problema mecânico AC (fora escopo ECU):
    → Gás R134a baixo (recarga)
    → Compressor desgastado (rebuild)
    → Evaporador bloqueado (limpeza)
    → Condensador sujo (limpeza)
    → Válvula expansão travada (trocar)

□ ECU OK se compressor engaging normalmente
```

#### 5.16.8 Custo Total AC

**ASPIRADO - Controle AC apenas (AC já existe):**
```
✅ Relé 30A:                    R$ 30
✅ Pressostato alta pressão:    R$ 120
✅ Fiação + conectores:         R$ 80
✅ Fusível inline:              R$ 15
--------------------------------------
TOTAL:                          R$ 245
```

**ASPIRADO - Sistema AC completo (se não tiver):**
```
✅ Controle ECU (acima):        R$ 245
✅ Compressor remanufaturado:   R$ 800-1,500
✅ Condensador:                 R$ 300-500
✅ Evaporador + caixa:          R$ 400-700
✅ Linhas + válvula expansão:   R$ 300-500
✅ Gás R134a + óleo PAG:        R$ 150-250
✅ Instalação profissional:     R$ 500-1,000
--------------------------------------
TOTAL COMPLETO:                 R$ 2,695-4,695

OU Kit AC completo:             R$ 2,500-3,500 (Technoauto/Metal Horse)
```

**TURBO 0.5 bar - Sem modificações:**
```
Custo AC:                       R$ 245 (controle apenas)
FMIC menor (350x250):           R$ 900-1,200
--------------------------------------
TOTAL:                          R$ 1,145-1,445
```

**TURBO 0.8 bar - Radiador offset:**
```
Custo AC:                       R$ 245
Radiador elétrico + fans:       R$ 1,200
--------------------------------------
TOTAL:                          R$ 1,445
```

**TURBO >1.0 bar - Compressor elétrico:**
```
Custo AC:                       R$ 245
Compressor elétrico 12V:        R$ 1,800
Alternador 70-90A:              R$ 600
--------------------------------------
TOTAL:                          R$ 2,645
```

### 5.17 CÂMBIO SEQUENCIAL + PADDLE SHIFT

**Sistema sequencial com flat shift integrado SCG-ECU**

#### 5.17.1 Visão Geral Sistema Sequencial

**O que é Sequential Shift?**

Câmbio sequencial permite trocar marchas sem usar embreagem (flat shifting) usando paddle shifters montados no volante, comum em carros de corrida, F1 e carros de alta performance.

**VANTAGENS:**
```
Operacionais:
  - Shift sem tirar mãos do volante
  - Troca sem embreagem (flat shift)
  - Mantém boost durante shift (turbo)
  - Shift mais rápido (~120-150ms vs 800-1200ms manual)
  - Menos erro humano (não pula marcha)
  - Controle preciso RPM durante shift

Performance:
  - Ganho tempo em pista: 0.3-0.5s por volta (circuito 60s)
  - Aceleração contínua sem perda boost
  - Menos desgaste embreagem
  - Launch control integrado
```

**DESVANTAGENS:**
```
  - Sistema complexo instalação
  - Custo elevado (R$ 3,500-5,500 profissional)
  - Manutenção especializada necessária
  - Desgaste câmbio acelerado (se mal configurado)
  - Curva aprendizado operação
  - Não recomendado trânsito pesado urbano
```

**COMPATIBILIDADE SCG-ECU:**
```
SCG-ECU JÁ TEM FLAT SHIFT IMPLEMENTADO!

Recursos disponíveis:
  - Spark cut programável (timing, percentual)
  - Fuel maintain (mantém boost turbo)
  - Paddle input detection (debounce)
  - Min RPM/TPS protection
  - Max shift time safety
  - Data logging completo
```

#### 5.17.2 Componentes Sistema Pneumático (RECOMENDADO)

**SISTEMA PROFISSIONAL - Pneumático**

**Atuador Pneumático Linear:**
```
Especificações:
  Tipo:               Push/pull linear
  Curso:              50-80mm (ajustável câmbio)
  Pressão operação:   6-8 bar
  Força saída:        200-400N
  Tempo resposta:     <50ms (mechanical)
  Material:           Alumínio anodizado + aço inox
  Montagem:           Bracket custom soldado
  Conexão:            Quick-disconnect 6mm

Marcas disponíveis:
  - Geartronics (importado)
  - Quaife (importado)
  - Pfitzner (importado)
  - Genéricos China (atenção qualidade)

Preço:                R$ 2,000-3,500 (conforme marca)
```

**Solenóides 3-way (Válvulas Pneumáticas):**
```
Especificações:
  Quantidade:         2x (UP shift + DOWN shift)
  Voltagem:           12V DC
  Corrente:           3-5A @ ativação
  Flow rate:          50-80 L/min
  Tempo resposta:     <10ms (elétrico)
  Tipo:               3-way normally closed
  Conexão:            Quick-disconnect 6mm
  Montagem:           Próximo pedais (routing curto)

Função:
  - Solenóide UP:     Direciona ar para push
  - Solenóide DOWN:   Direciona ar para pull
  - Ambos OFF:        Posição neutra mantida

Preço cada:           R$ 300-400
Total 2x:             R$ 600-800
```

**Fonte Pressão (2 opções):**

**OPÇÃO A: Compressor 12V (Elétrico)**
```
Especificações:
  Tipo:               Diafragma ou pistão
  Pressão máxima:     8-10 bar
  Vazão:              20-30 L/min
  Tanque pressão:     2-3 litros (reservatório)
  Duty cycle:         20% (intermitente - não contínuo)
  Consumo:            15-25A @ operação
  Montagem:           Engine bay
  Pressostato:        Incluso (auto on/off)
  Ruído:              Moderado (audível)

Vantagens:
  - Sempre disponível (não acaba)
  - Não requer recargas
  - Independente

Desvantagens:
  - Consumo elétrico alto
  - Alternador 60A+ necessário
  - Peso adicional
  - Ruído operação

Preço:                R$ 500-800
```

**OPÇÃO B: Cilindro CO2 (Gás Comprimido)**
```
Especificações:
  Tamanho cilindro:   500g-1kg (paintball/airsoft)
  Pressão saída:      Regulada 6-8 bar
  Autonomia:          ~500-1000 shifts (estimado)
  Regulador:          Necessário (pressão constante)
  Montagem:           Porta-malas ou banco traseiro
  Peso cheio:         ~1.5kg (1kg CO2)
  Recarga:            R$ 50-80 cada

Vantagens:
  - Mais leve que compressor
  - Sem consumo elétrico
  - Silencioso total
  - Resposta imediata

Desvantagens:
  - Recargas periódicas necessárias
  - Autonomia limitada (~1 mês track days)
  - Pode acabar em uso (backup?)

Preço cilindro:       R$ 300-500
Preço regulador:      R$ 150-250
Total setup:          R$ 450-750
```

**Paddle Switches (Botões Volante):**
```
Especificações:
  Tipo:               Microswitch SPST
  Montagem:           Volante (atrás - column mount)
  Quantidade:         2 (UP + DOWN)
  Rating:             5A @ 12V
  Debounce:           Hardware 10ms (capacitor)
  Conexão:            Spring contact column (rotativo)
  Material:           Alumínio ou plástico reforçado

Layout comum:
  - Paddle direito:   UP shift (index finger)
  - Paddle esquerdo:  DOWN shift (index finger)

Instalação:
  - Fixação abraçadeira coluna direção
  - Fiação passa por spring contact (airbag)
  - Ou sistema sem airbag (mais fácil)

Preço cada:           R$ 150-200
Total 2x:             R$ 300-400
```

**Tubulação Pneumática:**
```
Especificações:
  Material:           Nylon ou poliuretano (flexível)
  Diâmetro:           6mm OD / 4mm ID
  Pressão nominal:    15 bar (sobra margem)
  Comprimento total:  ~5 metros (depende routing)
  Conexões:           Quick-disconnect push-fit
  Cor:                Azul/preto (padronização)

Routing:
  Compressor/Cilindro → Solenóides → Atuador

  Linha 1 (UP):       Solenóide UP → Atuador push
  Linha 2 (DOWN):     Solenóide DOWN → Atuador pull
  Linha 3 (Supply):   Fonte → Distribuição solenóides

Preço metro:          R$ 15-30/metro
Total 5m:             R$ 80-150
```

**CUSTO TOTAL SISTEMA PNEUMÁTICO:**
```
Atuador pneumático:             R$ 2,500
2x Solenóides 3-way:            R$ 700
Compressor 12V OU Cilindro CO2: R$ 600 / R$ 600
2x Paddle switches:             R$ 400
Tubulação + fittings:           R$ 200
Bracket custom fabricação:      R$ 300
Fiação controle SCG-ECU:        R$ 150
Relés/fusíveis:                 R$ 100
-----------------------------------------------
TOTAL (compressor):             R$ 4,950

TOTAL (CO2):                    R$ 4,950
Instalação profissional:        +R$ 800-1,200
-----------------------------------------------
TOTAL INSTALADO:                R$ 5,750-6,150
```

#### 5.17.3 Integração SCG-ECU

**INPUTS (Paddle Switches):**

```ini
[PADDLE_INPUTS]
paddle_up_pin = PB4             # Shift UP (marcha acima)
paddle_down_pin = PB5           # Shift DOWN (marcha abaixo)
input_type = DIGITAL
polarity = ACTIVE_LOW           # Ground quando pressionado
pullup = INTERNAL               # Pull-up interno STM32 (10kΩ)
debounce_time = 20              # ms (filtro bounce mecânico)
```

**Fiação Inputs:**
```
PADDLE UP (direito):
  Terminal 1: SCG-ECU PB4
  Terminal 2: Ground (através spring contact)

PADDLE DOWN (esquerdo):
  Terminal 1: SCG-ECU PB5
  Terminal 2: Ground (através spring contact)

Spring Contact Column:
  - Permite rotação volante
  - 2 fios passam (UP + DOWN)
  - Ground comum chassis
  - Se sem airbag: fiação direta mais simples
```

**OUTPUTS (Solenóides):**

```ini
[SOLENOID_OUTPUTS]
solenoid_up_pin = PE6           # Solenóide UP shift
solenoid_down_pin = PE7         # Solenóide DOWN shift
output_type = PWM               # PWM para controle força
frequency = 100                 # Hz (standard)
pulse_width = 60                # ms (tempo ativação)
duty_cycle = 80                 # % (força aplicada)
max_pulse_time = 200            # ms (safety limit)
```

**Fiação Outputs:**
```
SOLENÓIDE UP:
  SCG-ECU PE6 → Relé pino 86 (controle)
  Relé pino 85: Ground
  Relé pino 30: Bateria 12V (fusível 20A)
  Relé pino 87: → Solenóide UP coil (+)
  Solenóide UP coil (−): → Ground

SOLENÓIDE DOWN:
  SCG-ECU PE7 → Relé pino 86 (controle)
  (mesma configuração acima)

Proteção:
  - Diodo flyback cada solenóide (1N4007)
  - Fusível 20A cada linha
  - Relé 30A automotive grade
```

**FLAT SHIFT CONFIGURATION:**

```ini
[FLAT_SHIFT]
enable = YES
activation = PADDLE_INPUT       # Trigger por paddle (não clutch)

[FLAT_SHIFT_TIMING]
spark_cut = 100                 # % (corta ignição 100% - full cut)
fuel_cut = 0                    # % (MANTER fuel - crítico turbo!)
cut_time = 120                  # ms (tempo corte ignição)
min_rpm = 3000                  # RPM mínimo para ativar
min_tps = 50                    # % acelerador mínimo
max_shift_time = 500            # ms (safety timeout)

[BOOST_MAINTAIN_TURBO]
enable = YES                    # *** CRÍTICO MOTORES TURBO ***
maintain_mode = FULL_FUEL       # Manter injeção 100%
target_boost_hold = YES         # Segurar wastegate posição
boost_recovery_fast = YES       # Recuperação rápida pós-shift
```

**EXPLICAÇÃO BOOST MAINTAIN:**
```
Problema sem boost maintain:
  1. Shift normal: Tira pé acelerador
  2. TPS fecha: Wastegate abre
  3. Boost cai: 1.0 bar → 0.2 bar
  4. Pós-shift: Delay ~1-2s rebuildar boost
  5. Perda tempo/performance

Solução boost maintain:
  1. Flat shift: NÃO tira pé acelerador
  2. Fuel mantido: Wastegate NÃO abre
  3. Spark cut apenas: Motor desacelera mas boost mantido
  4. Shift completo: Boost já em 1.0 bar
  5. Ganho: Resposta imediata pós-shift
```

#### 5.17.4 Sequência de Operação (Shift UP Exemplo)

**Timeline Detalhado:**

```
T = 0ms:
  - Motorista puxa paddle UP (dedo direito)
  - Microswitch fecha
  - SCG-ECU detecta PB4 → LOW

T = 20ms:
  - Debounce completo (filtro bounce)
  - SCG-ECU verifica condições:
    ✓ RPM >3000? SIM (4500 RPM atual)
    ✓ TPS >50%? SIM (85% WOT)
    ✓ Flat shift permitido? SIM

T = 25ms:
  - SCG-ECU inicia FLAT SHIFT
  - Corta ignição 100% (todas bobinas)
  - MANTÉM fuel 100% (todos injetores)
  - MANTÉM boost (wastegate travada)
  - Tempo previsto cut: 120ms

T = 30ms:
  - SCG-ECU ativa solenóide UP (PE6 → HIGH PWM 80%)
  - Relé fecha
  - Solenóide UP abre
  - Ar pressurizado (6-8 bar) → Atuador push

T = 50ms:
  - Atuador pneumático inicia movimento
  - Força aplicada alavanca câmbio
  - Começar desengatar marcha atual (3ª)

T = 70ms:
  - Marcha atual desengatada completamente
  - Câmbio em neutro momentâneo
  - Sincronizadores começam alinhar próxima marcha

T = 100ms:
  - Sincronizadores alinhados
  - Marcha 4ª engata
  - Resistência mecânica diminui

T = 110ms:
  - Marcha 4ª totalmente engatada
  - Atuador completou curso
  - SCG-ECU desativa solenóide (PE6 → LOW)

T = 120ms:
  - Cut time completo
  - SCG-ECU RETORNA IGNIÇÃO
  - Potência retorna IMEDIATAMENTE
  - Boost mantido em 1.0 bar (turbo)
  - RPM ajusta nova marcha (~3800 RPM)

T = 150ms:
  - Pressão pneumática retorna idle
  - Sistema pronto próximo shift
  - Driver continua acelerando normalmente

TOTAL TIME SHIFT: ~120-150ms
SHIFT MANUAL TÍPICO: ~800-1200ms

GANHO TEMPO: 6-8x mais rápido!
```

#### 5.17.5 Instalação Física

**Montagem Atuador Pneumático:**

```
Localização:
  - Alavanca câmbio (tunnel console)
  - Acesso direto haste shift
  - Próximo articulação câmbio

Bracket Custom:
  - Material: Aço 3-4mm espessura
  - Soldagem: TIG (alumínio) ou MIG (aço)
  - Montagem: Parafusos M8 ou M10
  - Design: Paralelo curso alavanca

Articulação:
  - Rod end bearing (rotula)
  - Permite movimento angular
  - Curso: 50-70mm (depende câmbio)
  - Ajuste: Rosca fine-tuning

Alinhamento crítico:
  - Atuador paralelo curso shift
  - Sem binding (travamento)
  - Força distribuída uniformemente
  - Teste movimento livre (sem pressão)

Segurança:
  - Trava mecânica (se pressão falhar)
  - Permite shift manual emergência
  - Parafusos autotravantes (Loctite)
```

**Montagem Paddle Switches:**

```
Localização volante:
  - Atrás volante (column mount)
  - Lado direito: UP shift
  - Lado esquerdo: DOWN shift
  - Alcance dedos indicadores

Fixação:
  - Abraçadeiras alumínio coluna direção
  - Ou suporte integrado volante (se aftermarket)
  - Ajuste angular conforme ergonomia

Fiação coluna:
  - COM airbag: Spring contact rotativo
  - SEM airbag: Fiação direta (mais simples)
  - Proteção: Sleeve térmico
  - Routing: Evitar interferência coluna

Teste operação:
  - Volante centro: Ambos paddles alcançáveis
  - Volante 90° direita: Paddle esquerdo OK
  - Volante 90° esquerda: Paddle direito OK
  - Força ativação: 2-4N (leve)
```

**Routing Tubulação Pneumática:**

```
Compressor/Cilindro: Engine bay ou porta-malas
  ↓ Tubulação 6mm supply
Distribuição: T-junction próximo solenóides
  ↓ ↓
Solenóide UP    Solenóide DOWN (próximo pedais)
  ↓               ↓
  └───────┬───────┘
         ↓ 2 linhas (push/pull)
    Atuador (tunnel câmbio)

Fixação tubulação:
  - Abraçadeiras P-clips cada 30cm
  - Evitar contato escape/superfícies quentes
  - Proteção: Sleeve térmico em áreas críticas
  - Folga movimento: Suspensão/motor
```

#### 5.17.6 Calibração e Ajustes

**Tuning Cut Time:**

```
Cut time muito curto (<80ms):
  Sintoma:
    - Marcha não engata
    - Grinding gears (dentes batendo)
    - Sincronizadores desgaste

  Solução:
    - Aumentar cut_time +20ms
    - Testar incrementalmente
    - [FLAT_SHIFT_TIMING] cut_time = 100

Cut time muito longo (>200ms):
  Sintoma:
    - RPM cai demais durante shift
    - Perda boost (turbo)
    - Shift "mole" (slow)

  Solução:
    - Reduzir cut_time -30ms
    - Objetivo: Mínimo necessário engatar
    - [FLAT_SHIFT_TIMING] cut_time = 140

IDEAL AP 1.8 Gol:
  - Câmbio original: 100-130ms
  - Câmbio reforçado (short shifter): 80-100ms
  - Câmbio dogbox (race): 60-80ms
```

**Tuning Pressão Pneumática:**

```
Pressão baixa (<5 bar):
  Sintoma:
    - Shift lento (>200ms)
    - Atuador "fraco"
    - Não completa curso

  Solução:
    - Aumentar pressão +1 bar
    - Verificar leaks (soap test)
    - Ajustar regulador

Pressão alta (>9 bar):
  Sintoma:
    - Shift violento (harsh)
    - Desgaste sincronizadores acelerado
    - Grinding ocasional

  Solução:
    - Reduzir pressão -1 bar
    - Suavizar entrada marcha

IDEAL:
  - 6-7 bar: Shift rápido mas suave
  - Testar incrementos 0.5 bar
```

**Tuning PWM Duty Cycle:**

```
[SOLENOID_OUTPUTS]
duty_cycle = 80    # % (padrão)

Duty baixo (<60%):
  - Solenóide resposta lenta
  - Shift inconsistente
  - Aumentar: duty_cycle = 75

Duty alto (>90%):
  - Aquecimento solenóide
  - Reduz vida útil
  - Consumo corrente alto
  - Reduzir: duty_cycle = 85

IDEAL: 75-85%
  - Resposta rápida
  - Temperatura OK
  - Confiabilidade
```

#### 5.17.7 Alternativa DIY Elétrico (Budget)

**SISTEMA ECONÔMICO - Servo Motor**

Se budget limitado, sistema elétrico com servo motor é alternativa:

**Componentes:**
```
Servo Motor High-Torque 12V:
  - Torque: 20-30 kgf.cm
  - Velocidade: 0.1s/60°
  - Tipo: DC motor + gearbox
  - Preço: R$ 150-300

Encoder Posição:
  - Tipo: Magnético ou hall effect
  - Resolução: 360 PPR
  - Feedback marcha atual
  - Preço: R$ 80-150

Arduino/Microcontroller:
  - Arduino Nano ou ESP32
  - Controla servo motor
  - Interface SCG-ECU (flat shift trigger)
  - Preço: R$ 50-80

Driver Servo:
  - H-bridge high current
  - Ou ESC brushed 30A
  - Preço: R$ 60-120

Paddle Switches:
  - Mesmos pneumático
  - Preço: R$ 300-400

Fiação + misc:
  - Preço: R$ 50-100

-----------------------------------------------
TOTAL SISTEMA DIY:            R$ 690-1,150
```

**Vantagens DIY Elétrico:**
```
  - MUITO mais barato (70% economia)
  - DIY friendly (maker)
  - Sem pressão/compressor/tubulação
  - Manutenção simples
  - Aprendizado eletrônica
```

**Desvantagens DIY Elétrico:**
```
  - Shift MAIS lento (~200-350ms vs 120ms)
  - Menos confiável (não race-ready)
  - Mais complexidade software
  - Não recomendado competição
  - Torque limitado (câmbio duros problema)
  - Desgaste servo motor (vida útil curta track)
```

**Recomendação DIY:**
```
Uso:
  - Aprendizado sistema
  - Testes iniciais
  - Street occasional fun
  - Budget muito limitado

NÃO usar para:
  - Competição
  - Track days intensos
  - Motor >200cv
  - Câmbios reforçados duros

Upgrade path:
  - Começar DIY elétrico
  - Validar conceito
  - Upgrade pneumático depois quando budget permitir
```

#### 5.17.8 Custo Total Resumo

**Sistema Pneumático Profissional:**
```
OPÇÃO COMPRESSOR 12V:
  Componentes:                    R$ 4,950
  Instalação profissional:        R$ 1,000
  -----------------------------------------------
  TOTAL:                          R$ 5,950

OPÇÃO CILINDRO CO2:
  Componentes:                    R$ 4,950
  Instalação profissional:        R$ 1,000
  Recargas CO2 (10x ano):         R$ 600/ano
  -----------------------------------------------
  TOTAL inicial:                  R$ 5,950
  TOTAL ano 1:                    R$ 6,550
```

**Sistema DIY Elétrico:**
```
  Componentes:                    R$ 1,000
  DIY install (você):             R$ 0
  -----------------------------------------------
  TOTAL:                          R$ 1,000
```

#### 5.17.9 Manutenção Periódica

**Semanal (Track days):**
```
  - Verificar pressão sistema (6-8 bar)
  - Verificar vazamentos (soap test conexões)
  - Verificar nível óleo compressor (se compressor)
  - Testar paddles resposta (no key - só elétrico)
  - Limpar sujeira atuador
```

**Mensal:**
```
  - Limpar filtro compressor ar (se compressor)
  - Verificar desgaste atuador (curso completo?)
  - Verificar apertar bracket (vibração afrouxa)
  - Verificar fiação paddle (flex repetitivo)
  - Grease articulação rod end bearing
```

**Semestral:**
```
  - Trocar óleo compressor (se compressor)
  - Verificar solenóides (rebuild se necessário)
  - Verificar sincronizadores câmbio (inspeção)
  - Re-calibrar timing se necessário
  - Verificar consumo corrente solenóides
```

**Anual:**
```
  - Rebuild completo atuador (selos O-ring)
  - Trocar tubulação se ressecada
  - Inspecionar bracket soldas
  - Verificar desgaste câmbio (diagnóstico)
```

### 5.18 COMPONENTES CRÍTICOS TURBO

**Componentes essenciais para turbo confiável e seguro**

#### 5.18.1 Oil Catch Can (OBRIGATÓRIO)

**PROBLEMA - Pressurização Cárter:**

Em motores turbo, a pressão no coletor de admissão aumenta significativamente durante boost. Esta pressão é transmitida ao cárter através do sistema PCV (Positive Crankcase Ventilation), forçando vapor de óleo misturado com blow-by de volta para o intake.

```
Processo:
  1. Turbo gera boost (0.5-1.0 bar)
  2. Boost enter intake manifold
  3. PCV valve conecta intake → cárter
  4. Pressão diferencial empurra gases cárter
  5. Vapor óleo + blow-by → intake
  6. Contamina intercooler e combustão
```

**CONSEQUÊNCIAS:**

```
Curto prazo:
  - Óleo contamina intercooler (reduz eficiência)
  - Óleo queima na combustão (fumaça azul escape)
  - Incrustações intake manifold (carbon buildup)
  - Smell óleo queimado

Médio prazo:
  - Octanagem efetiva reduzida (óleo = baixa octanagem)
  - Detonação facilitada (knock risk)
  - Perda potência (intercooler sujo)
  - Consumo óleo aumentado

Longo prazo:
  - Intercooler totalmente contaminado
  - Válvulas intake sujas (perda flow)
  - Sensor MAF contaminado (leitura errada)
  - Danos motor (knock severo)
```

**SOLUÇÃO - Oil Catch Can System:**

**Setup Recomendado (DUPLO):**

```
SISTEMA 1 - CCV (Crankcase Vent):
  Cárter → Catch Can #1 → Atmosfera (filtered)

  Função: Ventilação passiva
  Quando: Sempre (idle, cruise, WOT)
  Captura: Vapores óleo leves

SISTEMA 2 - PCV (Positive Crankcase Vent):
  Cárter → Catch Can #2 → Intake (pós-turbo)

  Função: Ventilação ativa (vácuo)
  Quando: Cruise (vácuo), não em boost
  Captura: Vapores óleo pesados + blow-by

Drenagem:
  - Drenar a cada 500-1000 km
  - Óleo capturado retorna ao cárter
  - Monitorar quantidade (diagnóstico motor)
```

**Componentes Catch Can:**

```
Reservatório:
  Material: Alumínio anodizado
  Capacidade: 500ml mínimo (1000ml ideal)
  Tampa: Rosqueada com O-ring
  Dreno: Válvula inferior (easy drain)
  Visor: Nível óleo (opcional mas útil)

Internos (Baffles):
  - Múltiplas camadas separação
  - Material: Bronze ou aço inox
  - Design: Turbulento (força condensação)
  - Eficiência: >95% captura óleo

Filtro Saída:
  - Filtro bronze mesh (CCV atmosfera)
  - Evita óleo sair pós-catch
  - Elemento lavável

Montagem:
  - Localização: Engine bay (acessível)
  - Suportes: Bracket anti-vibração
  - Altura: Acima cárter (gravidade ajuda)
  - Linhas: Mangueira AN-10 (10mm ID) ou silicone
```

**Marcas Disponíveis Brasil:**

```
NPL (Nacional):
  - Modelos: 500ml / 1000ml
  - Preço: R$ 300-500
  - Qualidade: Boa (entry-level)
  - Disponibilidade: Fácil

APR (Performance):
  - Modelos: Universal 800ml / 1200ml
  - Preço: R$ 500-800
  - Qualidade: Muito boa
  - Baffles: Multi-camada

Mishimoto (Importado):
  - Modelos: Compact / Large
  - Preço: R$ 600-1,000
  - Qualidade: Excelente
  - Design: Compacto eficiente

Radium Engineering (High-End):
  - Modelos: Dual-can setup
  - Preço: R$ 800-1,200 cada
  - Qualidade: Profissional race
  - Dipstick: Visor nível incluso
```

**Obrigatoriedade por Boost:**

```
ASPIRADO:
  - Oil catch can: Opcional
  - Benefício: Marginal (~2-3%)
  - Recomendação: Não prioritário

TURBO 0.3-0.5 bar:
  - Oil catch can: Recomendado
  - Benefício: Moderado (~10-15%)
  - Setup: Single CCV suficiente

TURBO 0.5-0.8 bar:
  - Oil catch can: MUITO recomendado
  - Benefício: Significativo (~20-30%)
  - Setup: Dual (CCV + PCV) ideal

TURBO >0.8 bar:
  - Oil catch can: OBRIGATÓRIO
  - Benefício: Crítico (>30%)
  - Setup: Dual profissional
  - Consequência sem: Danos motor prováveis
```

#### 5.18.2 Sensores EGT (Exhaust Gas Temperature)

**IMPORTÂNCIA CRÍTICA TURBO:**

EGT (Exhaust Gas Temperature) é **O INDICADOR MAIS IMPORTANTE** de condição do motor turbo. Monitora temperatura gases escape que entram na turbina.

**Por que crítico?**

```
Temperatura escape indica:
  - Riqueza mistura (AFR)
  - Advance timing (detonação)
  - Carga motor (load)
  - Saúde turbina (temperatura operação)
  - Eficiência combustão

Temperatura >900°C:
  - Risco dano válvulas escape
  - Risco dano turbina (blades)
  - Risco dano pistões (coroa)
  - Detonação provável
```

**Limites Seguros (Por Temperatura):**

```
<750°C:
  Status: SAFE (verde)
  Operação: Cruise normal
  Boost: Qualquer
  Duração: Ilimitada

750-850°C:
  Status: OK (amarelo)
  Operação: WOT momentâneo (10-15s)
  Boost: 0.8-1.0 bar
  Duração: Limitada (<30s)

850-900°C:
  Status: WARNING (laranja)
  Operação: WOT sustentado race
  Boost: 1.0+ bar
  Duração: Curta (<10s)
  Ação: Monitorar AFR, reduzir boost se persistir

>900°C:
  Status: DANGER (vermelho)
  Operação: RISCO DANO IMINENTE
  Boost: Qualquer
  Duração: ZERO (imediato)
  Ação: Enriquecer AFR -0.10 lambda, reduzir advance -5°

>950°C:
  Status: CRITICAL (piscando)
  Operação: DANO OCORRENDO
  Ação: CORTAR BOOST imediatamente, diagnosticar
```

**Componentes EGT System:**

**Termopar K-type (Sensor):**

```
Especificações:
  Tipo: K-type (Chromel-Alumel)
  Range: 0-1200°C (2192°F)
  Output: ~41 µV/°C (thermocouple voltage)
  Tempo resposta: <100ms (fast)
  Rosca: M8 ou M10x1.0
  Comprimento probe: 50-100mm
  Material: Inconel sheath (high temp)

Localização instalação:
  - Coletor escape (pré-turbo)
  - Distância port: 10-15cm
  - Penetração: 1/3 to 1/2 diâmetro tubo
  - Evitar: Contato direto parede tubo

Bung instalação:
  - Soldagem: TIG alumínio ou MIG aço
  - Ângulo: 45° (evita direto flow)
  - Thread: M8 ou M10 com O-ring
  - Torque: 15-20 N.m (não overtighten)

Preço cada: R$ 120-180
```

**Amplificador MAX31855 (Interface):**

```
Especificações:
  Chip: Maxim MAX31855
  Interface: SPI (Serial Peripheral)
  Resolução: 0.25°C (14-bit)
  Temperatura ambiente: -270°C to +1372°C
  Cold-junction comp: Sim (automático)
  Alimentação: 3.3V ou 5V
  Preço: R$ 30-50 (módulo breakout)

Conexão SCG-ECU:
  MAX31855 → SPI bus STM32
  - SCK: SPI Clock
  - MISO: Data output
  - CS: Chip select (1 por sensor)
  - GND/VCC: Power
```

**Configuração Multi-Sensor (RECOMENDADO):**

```
Setup Ideal - 4x Sensores (1 por cilindro):

Cilindro 1: Escape port 1 → Termopar → MAX31855 #1
Cilindro 2: Escape port 2 → Termopar → MAX31855 #2
Cilindro 3: Escape port 3 → Termopar → MAX31855 #3
Cilindro 4: Escape port 4 → Termopar → MAX31855 #4

Todos MAX31855 → SPI bus → SCG-ECU

Vantagem:
  - Detecta cilindro individual lean/rich
  - Identifica misfire
  - Diagnóstico precisão
  - EGT spread analysis

Setup Mínimo - 1x Sensor:

Cilindro 3 ou 4: Escape port → Termopar → MAX31855 → SCG-ECU

Vantagem:
  - Custo baixo
  - Informação geral OK
  - Melhor que nada

Desvantagem:
  - Não detecta problemas cilindro individual
```

**Configuração SCG-ECU:**

```ini
[EGT_SENSORS]
type = K_THERMOCOUPLE
count = 4                       # 1 por cilindro
amplifier = MAX31855
interface = SPI
spi_bus = SPI1
update_rate = 10                # Hz (10x por segundo)

[EGT_LIMITS]
max_continuous = 900            # °C (limite contínuo)
warning_temp = 850              # °C (warning start)
critical_temp = 920             # °C (critical danger)
shutdown_temp = 950             # °C (emergency shutdown)

[EGT_PROTECTION]
# Ação quando EGT >850°C
action = ENRICHMENT_20          # +20% fuel
retard = -3                     # -3° timing
boost_reduce = 20               # -20% boost target

# Ação quando EGT >900°C
action_critical = ENRICHMENT_30 # +30% fuel
retard_critical = -6            # -6° timing
boost_reduce_critical = 50      # -50% boost

# Ação quando EGT >950°C
action_shutdown = LIMP_MODE     # Cut boost completamente
notification = DASHBOARD_FLASH  # Flash warning
data_log = EMERGENCY_SAVE       # Save log imediato
```

**Instalação Física:**

```
Soldagem Bung:
  1. Marcar posição (10-15cm após port)
  2. Furar tubo escape (broca 7mm para M8)
  3. Soldar bung (TIG ideal, MIG OK)
  4. Limpar escória interna (importante!)
  5. Instalar termopar com anti-seize
  6. Torque 15-20 N.m (moderado)

Fiação Termopar:
  - NÃO usar cabo comum (erro leitura!)
  - Usar THERMOCOUPLE WIRE (específico K-type)
  - Comprimento max: 3 metros
  - Routing: Evitar interferência EMI
  - Proteção: Sleeve alumínio (heat shield)

Conexão Amplificador:
  - Montar em local fresco (não engine bay)
  - Firewall passenger side ideal
  - Dentro cabine protegido
  - Fiação SPI → SCG-ECU
```

**Custo Total EGT System:**

```
SETUP COMPLETO (4 sensores):
  4x Termopar K-type:           R$ 600
  4x MAX31855 amplifier:        R$ 150
  4x Bung M10:                  R$ 80
  4x Thermocouple wire (2m):    R$ 120
  Fiação SPI bus:               R$ 50
  Soldagem bungs:               R$ 200
  -------------------------------------
  TOTAL:                        R$ 1,200

SETUP MÍNIMO (1 sensor):
  1x Termopar K-type:           R$ 150
  1x MAX31855:                  R$ 40
  1x Bung M10:                  R$ 20
  Fiação:                       R$ 40
  Soldagem:                     R$ 50
  -------------------------------------
  TOTAL:                        R$ 300
```

#### 5.18.3 Sensor Oil Pressure (CRÍTICO TURBO)

**IMPORTÂNCIA:**

Turbocompressor requer pressão óleo **CONSTANTE e ADEQUADA** para lubrificação dos bearings (rolamentos). Sem pressão correta, bearing desgasta e turbina falha catastróficamente.

**Pressão Mínima Operação:**

```
Idle (850 RPM):
  - Pressão mínima: 1.0 bar (14 psi)
  - Pressão ideal: 1.5-2.0 bar
  - Abaixo 1.0 bar: WARNING (limp mode)

Cruise (2000 RPM):
  - Pressão mínima: 2.5 bar (36 psi)
  - Pressão ideal: 3.0-3.5 bar
  - Abaixo 2.5 bar: Investigar

WOT (5000+ RPM):
  - Pressão mínima: 4.0 bar (58 psi)
  - Pressão ideal: 4.5-5.5 bar
  - Acima 6.0 bar: Verificar válvula alívio

CRÍTICO:
  - Abaixo mínimo = DESLIGAR MOTOR IMEDIATAMENTE
  - Dano bearing turbo: R$ 1,500-3,000 (CHRA rebuild)
  - Dano motor: R$ 5,000-15,000 (rebuild completo)
```

**Sensor Recomendado:**

**VDO 360-004 (0-10 bar):**

```
Especificações:
  Range: 0-10 bar (0-145 psi)
  Output: 0.5-4.5V linear
  Precisão: ±1.5% full scale
  Rosca: M10x1.0 ou 1/8" NPT
  Alimentação: 5V DC
  Corrente: <15mA
  Temperatura operação: -40°C to +125°C
  Preço: R$ 180-250

Curva voltagem:
  0.5V = 0 bar
  1.5V = 2.5 bar
  2.5V = 5.0 bar
  3.5V = 7.5 bar
  4.5V = 10.0 bar
```

**Localização Instalação:**

```
OPÇÃO 1 (Ideal): Galeria óleo principal (bloco)
  - Acesso direto pressão motor
  - Leitura precisa
  - Requer T-adapter (manter switch original)

OPÇÃO 2: Linha alimentação turbo
  - Leitura pressão entrada turbo
  - Diagnóstico turbo específico
  - Mais fácil acesso

Instalação T-adapter:
  - Switch original pressão → T-adapter
  - Terminal 1: Switch original (warning lamp)
  - Terminal 2: Sensor VDO (SCG-ECU)
  - Tópico: M10x1.0 ou 1/8" NPT
  - Anti-seize: Aplicar rosca
  - Torque: 15-20 N.m
```

**Configuração SCG-ECU:**

```ini
[OIL_PRESSURE]
type = ANALOG
pin = PA6                       # Analog input
voltage_min = 0.5               # V
voltage_max = 4.5               # V
pressure_min = 0                # bar
pressure_max = 10               # bar
update_rate = 50                # Hz (50x/segundo)

[OIL_PROTECTION]
# Limites por RPM
min_pressure_idle = 1.0         # bar @ <1000 RPM
min_pressure_cruise = 3.0       # bar @ 1000-3000 RPM
min_pressure_wot = 4.0          # bar @ >3000 RPM

# Ações proteção
warning_pressure = 2.0          # bar (warning lamp)
critical_pressure = 1.5         # bar (buzzer)
shutdown_pressure = 1.0         # bar (limp mode)

[LIMP_MODE_ACTION]
action = REDUCE_POWER_50        # Reduz potência 50%
boost_cut = YES                 # Corta boost totalmente
max_rpm = 3000                  # Limita RPM
notification = DASHBOARD_FLASH  # Flash warning red
data_log = EMERGENCY_SAVE       # Save log

[SHUTDOWN_ACTION]
# Se pressão <1.0 bar por >3 segundos
action = ENGINE_SHUTDOWN        # Corta ignição + fuel
delay = 3                       # segundos (confirmar falha)
override = MANUAL_ONLY          # Só manual restart
```

**Troubleshooting Pressão Baixa:**

```
Pressão baixa idle (<1.0 bar):
  Causas:
    - Óleo nível baixo (CHECK FIRST!)
    - Óleo viscosidade errada (muito fino)
    - Bomba óleo desgastada
    - Válvula alívio travada aberta
    - Bearing desgaste (motor ou turbo)

Pressão baixa WOT (<4.0 bar):
  Causas:
    - Pickup óleo entupido (starving)
    - Bomba óleo insuficiente (upgrade?)
    - Vazamento linha turbo
    - Restrição linha retorno
    - Óleo muito quente (>120°C)

Pressão alta (>6.0 bar):
  Causas:
    - Válvula alívio travada fechada
    - Linha retorno bloqueada
    - Óleo muito frio (<40°C)
    - Óleo viscosidade muito alta
```

#### 5.18.4 Resumo Componentes Obrigatórios vs Recomendados

**OBRIGATÓRIOS TODO SETUP TURBO:**

```
Oil catch can (dual):           R$ 500-800
  └─ Previne danos intercooler/motor

Sensor oil pressure:            R$ 250
  └─ Previne danos turbo/motor

Wideband O2 (LSU 4.9):          R$ 1,000
  └─ Tuning preciso AFR

Knock sensor (mínimo 1):        R$ 200-400
  └─ Detecta detonação

MAP sensor 3-bar:               R$ 120
  └─ Leitura boost

-----------------------------------------------
TOTAL OBRIGATÓRIOS:             R$ 2,070-2,570
```

**MUITO RECOMENDADOS (Turbo >0.5 bar):**

```
4x Sensores EGT:                R$ 1,200
  └─ Monitoramento temperatura crítico

Comando válvulas turbo:         R$ 1,200
  └─ Overlap reduzido, mantém boost

Boost gauge (cockpit):          R$ 200-400
  └─ Feedback driver real-time

Oil temp gauge:                 R$ 150-250
  └─ Monitoramento saúde óleo

Intercooler temp sensor:        R$ 80-120
  └─ Eficiência intercooler

-----------------------------------------------
TOTAL RECOMENDADOS:             R$ 2,830-4,170
```

**TOTAL COMPONENTES CRÍTICOS:**

```
Obrigatórios:                   R$ 2,270
Recomendados:                   R$ 3,500
-----------------------------------------------
TOTAL:                          R$ 5,770

Vs dano motor sem:              R$ 15,000+
ROI proteção:                   260% (não fazer = burrice)
```

**Priorização Investimento (Budget Limitado):**

```
PRIORIDADE 1 (Não negocie):
  1. Wideband O2:               R$ 1,000
  2. Oil pressure sensor:       R$ 250
  3. Oil catch can:             R$ 500

PRIORIDADE 2 (Fortemente recomendado):
  4. Knock sensor:              R$ 400
  5. MAP 3-bar:                 R$ 120

PRIORIDADE 3 (Recomendado >0.5 bar):
  6. EGT sensors (mínimo 1):    R$ 300
  7. Comando turbo:             R$ 1,200

PRIORIDADE 4 (Nice to have):
  8. Gauges adicionais:         R$ 500
```

### 5.19 COMANDO DE VÁLVULAS - ASPIRADO vs TURBO

**Diferença crítica: Aspirado requer overlap alto, Turbo requer overlap baixo**

#### 5.19.1 Fundamentos Comando Válvulas

**O que é LSA (Lobe Separation Angle)?**

LSA é o ângulo entre o pico de lift (abertura máxima) da válvula de admissão e o pico de lift da válvula de escape.

```
LSA Menor (104-108°):
  Características:
    - Overlap maior (válvulas abertas simultaneamente)
    - Mais potência top-end (RPM alto)
    - Idle instável ("cam lope")
    - Vácuo baixo

  Uso:
    - Aspirado race
    - Motor >6000 RPM
    - Não recomendado turbo

LSA Maior (110-114°):
  Características:
    - Overlap menor
    - Idle estável
    - Vácuo alto
    - Torque médio

  Uso:
    - Daily driver
    - Turbo (IDEAL!)
    - Street performance
```

**O que é Overlap?**

Overlap é o período (em graus de virabrequim) onde válvula de admissão E válvula de escape estão abertas simultaneamente, próximo ao TDC (Top Dead Center) entre escape e admissão.

```
Aspirado - Overlap Alto (>40°):
  Benefício:
    - Scavenging effect (gases escape "puxam" admissão)
    - Melhor enchimento cilindro
    - Mais potência top-end

  Problema turbo:
    - Boost escapa pelo escape durante overlap
    - Perda pressão admissão
    - Eficiência reduzida

Turbo - Overlap Baixo (<38°):
  Benefício:
    - Mantém boost (não escapa)
    - Backpressure isolado do intake
    - Torque baixo-médio melhorado

  Problema aspirado:
    - Perda scavenging
    - Enchimento cilindro reduzido
```

**Por que Turbo Diferente?**

```
Motor Aspirado:
  Intake pressure: 0.3-0.8 bar (vácuo)
  Exhaust pressure: 1.0-1.1 bar
  Delta pressure: Escape > Intake (~0.5 bar)

  Durante overlap (ex: 44°):
    - Escape ainda expelindo (pressão alta)
    - Intake começando sugar (vácuo)
    - Flow: Escape → Intake → Escape (scavenging)
    - RESULTADO: Bom (limpa câmara)

Motor Turbo:
  Intake pressure: 1.5-2.0 bar (boost)
  Exhaust pressure: 2.0-2.5 bar (backpressure turbo)
  Delta pressure: Escape > Intake (~0.5-1.0 bar)

  Durante overlap (ex: 44°):
    - Escape ainda pressurizado
    - Intake JÁ pressurizado (boost)
    - Flow: Intake → Escape (PERDA BOOST!)
    - RESULTADO: Ruim (perde boost construído)

Conclusão:
  Turbo precisa FECHAR escape antes abrir intake!
  = Overlap mínimo
  = LSA maior (111-114°)
```

#### 5.19.2 Comandos Aspirado (Mercado Brasil)

**49G Original (Stock VW AP 1.8):**

```
Especificações:
  Duration intake: 240°
  Duration exhaust: 266°
  Lift intake: 10.3mm
  Lift exhaust: 10.8mm
  LSA: 106° (relativamente baixo)
  Overlap: 44° (alto para turbo!)

Características:
  - Idle: Estável
  - Vácuo: Alto (~550 mbar idle)
  - Torque baixo: Excelente
  - Potência: Até 110cv (com SCG-ECU)

Uso recomendado:
  - Daily driver aspirado
  - Turbo conservador <0.5 bar (aceita)

Preço:
  - Original usado: R$ 300-500
```

**Comando 276° (Street/Track Mild):**

```
Especificações:
  Duration intake: 276°
  Duration exhaust: 276°
  Lift intake: 11.0mm
  Lift exhaust: 11.0mm
  LSA: 108°
  Overlap: 48° (aumentado)

Características:
  - Idle: Levemente instável (acceptable)
  - Vácuo: Médio (~450 mbar idle)
  - Torque baixo: Reduzido (-10%)
  - Torque médio-alto: Aumentado (+15%)
  - Potência: Até 130cv aspirado

Uso recomendado:
  - Street performance
  - Pista ocasional
  - NÃO turbo (overlap muito alto)

Preço: R$ 600-900
```

**Comando 288°-296° (Race Agressivo):**

```
Especificações:
  Duration: 288°-296° (extremo)
  Lift: 11.5-12.0mm
  LSA: 106° (baixo)
  Overlap: 54°+ (muito alto)

Características:
  - Idle: MUITO instável (lope severo)
  - Vácuo: Baixo (<350 mbar)
  - Torque <2500 RPM: Fraco (-30%)
  - Potência >5500 RPM: Alto (+25%)
  - Potência máxima: 145cv+ aspirado

Uso:
  - Race dedicado
  - Pista apenas
  - NÃO daily driver
  - NUNCA turbo

Preço: R$ 800-1,200
```

#### 5.19.3 Comandos Turbo (Mercado Brasil)

**SPA Turbo 268° (RECOMENDADO - Melhor Custo-Benefício):**

```
Especificações:
  Duration intake: 268°
  Duration exhaust: 272°
  Lift intake: 10.5mm
  Lift exhaust: 10.8mm
  LSA: 112° (ALTO - ideal turbo)
  Overlap: 34° (BAIXO - mantém boost)

Características:
  - Idle: Leve instabilidade (aceitável daily)
  - Vácuo: Médio-Alto (~480 mbar)
  - Torque baixo-médio: Excelente (+20% vs 49G)
  - Response turbo: Rápida (low lag)
  - Potência máxima: 300cv+ turbo

Vantagens:
  - Mantém boost durante overlap
  - Response turbo melhorada
  - Torque desde 2000 RPM
  - Idle aceitável daily
  - Preço acessível nacional

Desvantagens:
  - Idle não 100% smooth
  - Consumo ligeiramente maior

Uso recomendado:
  - Turbo 0.5-1.0 bar (IDEAL)
  - Street + track
  - Daily driver turbo

Preço: R$ 800-1,200
Instalação: R$ 400-600
TOTAL: R$ 1,200-1,800
```

**Crower Turbo 264° (Daily Driver Perfeito):**

```
Especificações:
  Duration intake: 264°
  Duration exhaust: 270°
  Lift: 10.2mm / 10.6mm
  LSA: 114° (MUITO ALTO)
  Overlap: 28° (MUITO BAIXO)

Características:
  - Idle: Quase normal (smooth)
  - Vácuo: Alto (~520 mbar)
  - Consumo: Similar original
  - Boost mantido: Máximo (28° overlap)
  - Potência máxima: 250cv turbo

Vantagens:
  - Idle melhor que 49G original
  - Daily driver perfeito
  - Consumo OK
  - Boost máximo mantido

Desvantagens:
  - Potência top-end limitada vs SPA 268°
  - Preço mais alto (importado)
  - Disponibilidade difícil Brasil

Uso:
  - Turbo daily driver
  - 0.5-0.8 bar street

Preço: R$ 1,200-1,800 (importado)
```

**Comp Cams Turbo 276° (Race High Power):**

```
Especificações:
  Duration: 276° / 280°
  Lift: 11.2mm / 11.5mm
  LSA: 110° (moderado turbo)
  Overlap: 38° (borderline)

Características:
  - Idle: Instável (race)
  - Potência: 350cv+ turbo
  - Flow: Muito alto
  - Boost: Perda leve overlap (aceita)

Uso:
  - Race turbo >1.0 bar
  - Competição
  - NÃO daily

Preço: R$ 1,500-2,200 (importado)
```

#### 5.19.4 Recomendações por Setup e Boost

**ASPIRADO (por objetivo):**

```
Daily Driver (97-110cv):
  Comando: 49G original
  Custo: R$ 0 (já instalado)
  Idle: Perfeito
  Consumo: Ótimo

Street Performance (110-130cv):
  Comando: 276° preparado
  Custo: R$ 600-900 + instalação R$ 500
  Idle: Aceitável
  Consumo: +10%

Race (130-145cv):
  Comando: 288° agressivo
  Custo: R$ 800-1,200 + instalação R$ 500
  Idle: Ruim (apenas pista)
  Consumo: +20%
```

**TURBO (por nível boost):**

```
0.3-0.5 bar (140-160cv):
  Opção 1: 49G original (economizar)
    - Funciona OK
    - Perda boost ~5% em overlap
    - Custo: R$ 0

  Opção 2: SPA 268° (melhor)
    - Ganho ~5-8% potência
    - Response melhor
    - Custo: R$ 1,200-1,800

  Recomendação: 49G OK se budget apertado
                SPA 268° se pode investir

0.5-0.8 bar (160-200cv):
  Comando: SPA 268° turbo
  Custo: R$ 1,200-1,800
  Benefício: +10-15% vs 49G

  Recomendação: Vale MUITO investimento

0.8-1.2 bar (200-300cv):
  Opção 1: SPA 268° (street/track)
  Opção 2: Crower 264° (daily smooth)
  Custo: R$ 1,200-1,800

  Recomendação: SPA 268° melhor geral

>1.2 bar race (>300cv):
  Comando: Comp Cams 276° turbo
  Custo: R$ 1,500-2,200
  Uso: Race apenas

  Recomendação: Só se meta >300cv
```

#### 5.19.5 Instalação Comando Válvulas

**Mão de Obra Profissional:**

```
Serviço completo inclui:
  1. Remoção correia dentada
  2. Remoção tampa válvulas
  3. Remoção comando antigo
  4. Instalação comando novo
  5. Calibração folga válvulas
  6. Sincronismo polia comando
  7. Instalação correia nova
  8. Teste funcionamento

Tempo: 4-6 horas (depende experiência)

Custo mão de obra: R$ 400-600
```

**Componentes Necessários Trocar:**

```
OBRIGATÓRIOS:
  Correia dentada nova:         R$ 80-120
    - Nunca reusar correia velha!
    - Gates ou Continental

  Tensor correia:               R$ 150-220
    - Automático ou manual
    - Verificar desgaste

  Junta tampa válvulas:         R$ 30-50
    - Sempre trocar (vedação)

  Óleo motor troca:             R$ 120-200
    - 5W40 ou 10W40 turbo
    - 4 litros

RECOMENDADOS:
  Polia comando nova:           R$ 100-150
    - Se desgastada (verificar)

  Retentores haste válvula:     R$ 120-180
    - Se >100,000 km

-----------------------------------------------
TOTAL componentes:              R$ 380-720
```

**CUSTO TOTAL UPGRADE COMANDO:**

```
EXEMPLO: Trocar 49G → SPA 268° Turbo

Comando SPA 268°:               R$ 1,000
Mão de obra instalação:         R$ 500
Correia dentada Gates:          R$ 100
Tensor automático:              R$ 200
Junta tampa válvulas:           R$ 40
Óleo Mobil 1 5W40 (4L):         R$ 180
-----------------------------------------------
TOTAL:                          R$ 2,020

Ganho potência (turbo 0.8 bar):
  Antes (49G):    ~170cv
  Depois (SPA):   ~185cv
  Ganho:          +15cv (+8.8%)

  R$/cv:          R$ 135 por cv adicional
```

#### 5.19.6 Quando NÃO Trocar Comando

**Situações Economizar:**

```
Turbo conservador <0.5 bar:
  - 49G original funciona OK
  - Perda performance marginal (<5%)
  - Economizar R$ 2,000
  - Upgrade quando aumentar boost

Budget muito limitado:
  - Priorizar outros componentes primeiro:
    1. Wideband O2 (R$ 1,000)
    2. Oil pressure sensor (R$ 250)
    3. Oil catch can (R$ 500)
    4. Knock sensor (R$ 400)
  - Comando vem depois quando sobrar

Daily driver suave:
  - Se prioridade é conforto idle
  - 49G melhor que qualquer preparado
  - Menos consumo

Primeiro turbo (validação):
  - Validar setup completo primeiro
  - Ver se gosta sistema
  - Depois upgrade comando
```

**Ordem Prioridade Investimento Turbo:**

```
FASE 1 (Setup funcional):
  1. Turbo + wastegate:         R$ 4,500
  2. Injetores 440cc:           R$ 750
  3. Bomba combustível 255L/h:  R$ 500
  4. Intercooler FMIC:          R$ 1,400
  SUBTOTAL:                     R$ 7,150

FASE 2 (Segurança):
  5. Wideband O2 LSU 4.9:       R$ 1,000
  6. Oil catch can:             R$ 500
  7. Oil pressure sensor:       R$ 250
  8. Knock sensor:              R$ 400
  SUBTOTAL:                     R$ 2,150

FASE 3 (Otimização):
  9. Comando turbo SPA 268°:    R$ 2,000
  10. Sensores EGT (4x):        R$ 1,200
  SUBTOTAL:                     R$ 3,200

TOTAL INVESTIMENTO:             R$ 12,500
```

**Comando é item #9 - Não é prioridade inicial!**

#### 5.19.7 Comparativo Final Aspirado vs Turbo

**TABELA COMPARATIVA:**

```
┌────────────────────────┬──────────────┬──────────────┐
│ Especificação          │ Aspirado     │ Turbo        │
├────────────────────────┼──────────────┼──────────────┤
│ LSA (Lobe Separation)  │ 104-108°     │ 111-114°     │
│ Overlap                │ 44-54°       │ 28-38°       │
│ Duration               │ 276-296°     │ 264-276°     │
│ Lift                   │ 11-12mm      │ 10-11mm      │
│ Idle quality           │ Pior         │ Melhor       │
│ Vácuo idle             │ Baixo        │ Alto         │
│ Torque baixo RPM       │ Pior         │ Melhor       │
│ Potência top-end       │ Melhor       │ Adequado     │
│ Consumo combustível    │ Maior        │ Menor        │
│ Daily drivability      │ Pior         │ Melhor       │
│ Manutenção boost       │ N/A          │ Crítico      │
└────────────────────────┴──────────────┴──────────────┘
```

**RESUMO ESCOLHA:**

```
Você tem motor ASPIRADO?
  → Comando duration alto (276-296°)
  → LSA baixo (104-108°)
  → Overlap alto (44-54°)
  → Exemplos: 276°, 288°, 296° race

Você tem motor TURBO?
  → Comando duration moderado (264-276°)
  → LSA ALTO (111-114°)
  → Overlap BAIXO (28-38°)
  → Exemplo: SPA 268° turbo (MELHOR!)

Você vai TROCAR aspirado → turbo?
  → Manter 49G original até boost >0.5 bar
  → Depois upgrade SPA 268° turbo
  → Não compre comando aspirado race!
```

---

## 6. SENSORES E CALIBRAÇÃO

### 6.1 Sensores Obrigatórios

**ASPIRADO:**
```
✅ TPS (Throttle Position)
✅ MAP (Manifold Pressure)
✅ CLT (Coolant Temp)
✅ IAT (Intake Air Temp)
✅ Lambda/O2 Wideband (LSU 4.9)
✅ Hall/Roda Fônica (Crank position)
✅ Cam sensor (opcional - sequential)
```

**TURBO (adicionar):**
```
✅ Knock sensor (1-4x)
✅ Oil pressure sensor
✅ EGT sensor (1-4x) - Exhaust Gas Temp
✅ Boost pressure (confirmar MAP range)
✅ Fuel pressure sensor (opcional)
```

### 6.2 TPS Calibration

**Sensor TPS Original VW:**
```
Tipo:               Potenciômetro 3 fios
Pinos:
  1: 5V (alimentação)
  2: Sinal (wiper)
  3: GND

Resistência total:  ~5kΩ
```

**Calibração SCG-ECU:**
```ini
[TPS_CALIBRATION]
adc_min = 50                # ADC count @ throttle closed
adc_max = 973               # ADC count @ WOT
voltage_min = 0.25          # V @ closed
voltage_max = 4.75          # V @ WOT

deadband = 2                # % (zona morta)
```

**Procedimento Calibração:**
```
1. Acelerador totalmente fechado
2. TunerStudio: "Calibrate TPS Closed"
3. Acelerador totalmente aberto (WOT)
4. TunerStudio: "Calibrate TPS Open"
5. Verificar % TPS vs posição física
```

### 6.3 MAP Sensor

**Original VW (Frequência):**
```
Tipo:               MAP sensor de frequência
Range:              20-105 kPa
Output:             80.9-162.4 Hz
Alimentação:        5V ± 5%
```

**UPGRADE TURBO - MAP 3-bar:**
```
Sensor:             GM 3-bar MAP (12223861)
Tipo:               Analógico (voltagem)
Range:              0-300 kPa (0-3 bar absolute)
Output:             0.5-4.5V
Custo:              ~R$ 120

Calibração:
  0.5V  = 0 kPa    (vácuo perfeito)
  1.5V  = 50 kPa   (meio vácuo)
  2.5V  = 100 kPa  (atmosférico)
  3.5V  = 200 kPa  (1 bar boost)
  4.5V  = 300 kPa  (2 bar boost)
```

**Configuração SCG-ECU:**
```ini
[MAP_SENSOR]
type = ANALOG               # GM 3-bar
adc_min = 102               # 0.5V
adc_max = 921               # 4.5V
kpa_min = 0
kpa_max = 300

# Ou FREQUENCY (original VW)
type = FREQUENCY
freq_min = 80.9             # Hz @ 20 kPa
freq_max = 162.4            # Hz @ 100 kPa
```

### 6.4 CLT/IAT Sensors

**Sensor NTC VW Original:**
```
Tipo:               NTC (Negative Temperature Coefficient)
Pullup resistor:    2490 Ω (SCG-ECU interno)
Curva:              Steinhart-Hart
```

**Tabela Resistência x Temperatura:**

| Temp (°C) | Resistência (Ω) | Voltagem 5V (V) |
|-----------|-----------------|-----------------|
| -40       | 100,000         | 4.95            |
| -20       | 25,000          | 4.70            |
| 0         | 6,000           | 4.20            |
| 20        | 2,500           | 3.35            |
| 40        | 1,000           | 2.20            |
| 60        | 500             | 1.45            |
| 80        | 300             | 1.00            |
| 100       | 180             | 0.65            |
| 120       | 110             | 0.45            |

**Configuração SCG-ECU:**
```ini
[CLT_SENSOR]
type = NTC
pullup_resistor = 2490      # Ω
bias_resistor = 2490        # Ω (mesmo valor)

# Steinhart-Hart coefficients (VW curve)
coeff_A = 0.001129241
coeff_B = 0.000234831
coeff_C = 0.000000085928

# Ou usar tabela lookup
use_table = YES
table_file = "vw_ntc_curve.csv"

[IAT_SENSOR]
# Mesmos parâmetros CLT
type = NTC
pullup_resistor = 2490
bias_resistor = 2490
```

### 6.5 Lambda/O2 Sensor

**CRÍTICO: Usar Wideband!**

**Narrowband Original (NÃO usar):**
```
Tipo:               Zircônia narrowband
Range:              0.1-0.9V (lambda 0.9-1.1)
Problema:           Só preciso em lambda 1.0
                    Inútil para tuning turbo!
```

**Wideband Recomendado:**
```
Sensor:             Bosch LSU 4.9
Controller:         AEM 30-2001 ou similar
Range:              Lambda 0.68-1.36 (AFR 10-20)
Output:             0-5V linear
Aquecimento:        Sim (opera <30s)
Custo:              ~R$ 800-1200

Instalação:
  - Escape: 50-80cm após coletor
  - Ângulo: 10-45° (evitar condensação)
  - Bung: M18x1.5 (soldar escape)
```

**Configuração SCG-ECU:**
```ini
[O2_SENSOR]
type = WIDEBAND
controller = AEM_UEGO       # Ou genérico 0-5V

# Calibração
voltage_min = 0.5           # V (lambda 0.68)
voltage_stoich = 2.5        # V (lambda 1.00)
voltage_max = 4.5           # V (lambda 1.36)

lambda_min = 0.68
lambda_max = 1.36

# Closed loop
enable_closed_loop = YES
activation_delay = 60       # segundos (aquecimento)
min_rpm = 1000
max_rpm = 4000              # Só cruise (não WOT)
```

### 6.6 Knock Sensor (Turbo)

**Sensor Bosch Original:**
```
Part number:        0 261 231 006
Tipo:               Piezoelétrico
Frequência:         5-15 kHz
Sensibilidade:      ~30 mV/g
Impedância:         4.5 MΩ
Montagem:           M8x1.25 (bloco motor)
Torque:             20 Nm
```

**Posicionamento:**
```
Cilindro único:     Entre cilindros 2-3
Multi cilindro:     1 por cilindro (ideal)
Altura:             Meio do bloco (water jacket)
Cabo:               Blindado até ECU (<1m)
```

**Configuração SCG-ECU:**
```ini
[KNOCK_SENSOR]
type = ANALOG_PIEZO
count = 1                   # Ou 4
amplification = 100         # Ganho

# Filtro passa-banda
freq_min = 5000             # Hz
freq_max = 15000            # Hz
freq_peak = 7500            # Hz (AP 1.8 típico)

# Detecção
threshold_voltage = 2.5     # V
threshold_adaptive = YES    # Auto-ajuste ruído
window_start = 350          # graus (após ignição)
window_duration = 40        # graus
```

### 6.7 Oil Pressure (Turbo)

**Sensor Pressão Óleo:**
```
Tipo:               Resistivo ou 0-5V
Range:              0-10 bar
Exemplo:            VDO 360-004 (0-10 bar)
Output:             0.5-4.5V linear
Rosca:              M10x1.0 ou NPT 1/8"
Localização:        Galeria óleo principal
```

**Configuração:**
```ini
[OIL_PRESSURE]
type = ANALOG
voltage_min = 0.5           # V = 0 bar
voltage_max = 4.5           # V = 10 bar
bar_min = 0
bar_max = 10

# Limites
min_idle = 1.0              # bar @ idle
min_cruise = 3.0            # bar @ cruise
warning = 2.0               # bar
critical = 1.5              # bar (limp mode)
```

### 6.8 EGT Sensors (Turbo)

**Termopar Tipo K:**
```
Tipo:               K-type thermocouple
Range:              0-1200°C
Output:             ~41 µV/°C
Necessita:          Amplificador (MAX6675 ou MAX31855)
Localização:        Escape pré-turbo
Distância:          10-15cm do port
Bung:               M8 ou M10 (soldar)
```

**Configuração:**
```ini
[EGT_SENSORS]
type = K_THERMOCOUPLE
count = 4                   # 1 por cilindro (ideal)
amplifier = MAX31855        # Chip SPI

# Limites
max_continuous = 900        # °C
warning = 850               # °C
critical = 920              # °C
action = ENRICHMENT_20      # +20% fuel
```

---

## 7. TABELAS DE MAPEAMENTO

### 7.1 VE Table (Volumetric Efficiency)

**Aspirado - Tabela Base 16x16:**

```
# VE Table - AP 1.8 Aspirado
# RPM (linhas) x MAP kPa (colunas)
# Valores em % (eficiência volumétrica)

RPM\MAP   20    30    40    50    60    70    80    90   100
 800      40    45    50    55    60    65    68    70    72
1000      42    47    52    57    62    67    70    73    75
1500      45    50    55    60    65    70    74    77    80
2000      48    53    58    63    68    73    77    81    84
2500      50    55    60    65    70    75    80    84    87
3000      52    57    62    67    72    77    82    86    90
3500      53    58    63    68    73    78    83    87    91
4000      54    59    64    69    74    79    84    88    92
4500      54    59    64    69    74    79    84    88    92
5000      53    58    63    68    73    78    83    87    91
5500      52    57    62    67    72    77    82    86    90
6000      50    55    60    65    70    75    80    84    87
6500      48    53    58    63    68    73    77    81    84
```

**Turbo - Tabela Base 16x16:**

```
# VE Table - AP 1.8 Turbo
# Valores MAIORES em boost (MAP >100 kPa)

RPM\MAP   20    40    60    80   100   120   140   160   180   200
 800      40    50    60    68    72    75    78    80    82    83
1000      42    52    62    70    75    78    81    83    85    86
1500      45    55    65    74    80    83    86    88    90    92
2000      48    58    68    77    84    88    91    94    96    98
2500      50    60    70    80    87    91    95    98   100   102
3000      52    62    72    82    90    95    99   103   106   108
3500      53    63    73    83    91    96   101   105   108   110
4000      54    64    74    84    92    98   103   107   110   112
4500      54    64    74    84    92    98   103   107   110   112
5000      53    63    73    83    91    97   102   106   109   111
5500      52    62    72    82    90    96   101   105   108   110
6000      50    60    70    80    87    93    98   102   105   107
6500      48    58    68    77    84    90    95    99   102   104
```

**NOTA VE Turbo:**
- VE >100% é NORMAL em boost (supercharging effect)
- VE 110% @ 1 bar boost = +10% massa ar vs aspirado
- Ajustar no dyno com wideband

### 7.2 Ignition Advance Table

**Ver seções 4.8 (Aspirado) e 5.10 (Turbo)**

### 7.3 AFR Target Table

**Ver seções 4.9 (Aspirado) e 5.11 (Turbo)**

### 7.4 Dwell Table (Voltage Compensated)

```
# Dwell vs Battery Voltage
# Valores em ms (tempo saturação bobina)

Voltage (V)   Dwell (ms)   Notas
-----------   ----------   -------------------------
    10.0         4.5       Bateria fraca (cranking)
    11.0         4.0
    12.0         3.5       Motor desligado
    13.0         3.2
    14.0         3.0       Normal (motor ligando)
    14.5         2.9
    15.0         2.8       Alternador alto
    16.0         2.8       Limite superior
```

### 7.5 Warmup Enrichment (WUE)

```
# Warmup Enrichment vs CLT
# Valores em % (100% = sem enriquecimento)

CLT (°C)    Enrichment (%)    Notas
--------    --------------    -----------------------
  -20           200           Motor muito frio
  -10           180
    0           150           Inverno típico
   10           135
   20           125           Temperatura ambiente
   30           115
   40           110           Aquecendo
   50           105
   60           102           Quase quente
   70           101
   80           100           Operação normal
   90           100
  100           100
```

### 7.6 Afterstart Enrichment (ASE)

```
# ASE vs Time after start
# Valores em % adicional

Time (s)    Enrichment (%)    Notas
--------    --------------    -----------------------
    0           150           Partida imediata
    1           130
    2           115
    3           105
    5           100           Volta ao normal
   10           100
```

### 7.7 Acceleration Enrichment

```
# AE - TPS Rate of Change
# Delta TPS (%/s) vs Enrichment (%)

TPS Rate     Enrichment     Decay Time
(/s)         (%)            (ms)
--------     ----------     ----------
    5           10            500
   10           25            600
   20           50            700
   50          100            800
  100          150           1000
```

---

## 8. PINOUT E FIAÇÃO

### 8.1 SCG-ECU Pinout (STM32F407VGT6)

**IMPORTANTE:** Pinout específico da placa SCG-ECU 2.0 (revisar esquemático real).

**Entradas Analógicas (ADC):**
```
PIN     FUNÇÃO              CONEXÃO GOL
----    ----------------    ---------------------------
PA0     TPS                 Sensor TPS pino 2 (sinal)
PA1     MAP (analog)        MAP sensor output (se analog)
PA2     CLT                 Sensor temperatura motor
PA3     IAT                 Sensor temperatura ar
PA4     O2/Lambda           Wideband controller output
PA5     Battery Voltage     Bateria via divisor 1:4
PA6     Oil Pressure        Sensor pressão óleo (turbo)
PA7     Fuel Pressure       Sensor pressão combustível
```

**Entradas Digitais:**
```
PIN     FUNÇÃO              CONEXÃO GOL
----    ----------------    ---------------------------
PB0     Crank (Hall/VR)     Sensor Hall distribuidor OU roda fônica
PB1     Cam (Hall)          Sensor fase comando (sequential)
PB2     Knock 1             Sensor knock cil 1-2
PB3     Knock 2             Sensor knock cil 3-4
PB4     Clutch Switch       Interruptor embreagem (launch/flat shift)
PB5     Brake Switch        Interruptor freio
```

**Saídas Injeção (Low-side):**
```
PIN     FUNÇÃO              CONEXÃO GOL
----    ----------------    ---------------------------
PC0     Injetor 1           Cilindro 1
PC1     Injetor 2           Cilindro 2
PC2     Injetor 3           Cilindro 3
PC3     Injetor 4           Cilindro 4
PC4     Injetor 5           (reserva/staging)
PC5     Injetor 6           (reserva/staging)
```

**Saídas Ignição (Low-side):**
```
PIN     FUNÇÃO              CONEXÃO GOL
----    ----------------    ---------------------------
PD0     Ignição 1           Bobina cil 1 (ou distribuidor)
PD1     Ignição 2           Bobina cil 2
PD2     Ignição 3           Bobina cil 3
PD3     Ignição 4           Bobina cil 4
```

**Saídas PWM (Auxiliares):**
```
PIN     FUNÇÃO              CONEXÃO GOL
----    ----------------    ---------------------------
PE0     Idle Control        Atuador marcha lenta (stepper/PWM)
PE1     Boost Control       Solenoid boost (turbo)
PE2     VVT Control         Controle VVT (futuro)
PE3     Fan Control         Eletroventilador PWM
PE4     Fuel Pump           Relé bomba combustível
```

**Comunicação:**
```
PIN     FUNÇÃO              CONEXÃO
----    ----------------    ---------------------------
PA9     UART1 TX            TunerStudio/PC (USB)
PA10    UART1 RX            TunerStudio/PC
PB6     CAN_RX              CAN bus (data logging/display)
PB7     CAN_TX              CAN bus
```

### 8.2 Fiação Gol 1994 → SCG-ECU

**ESQUEMA SIMPLIFICADO:**

```
CHICOTE MOTOR GOL 1994
----------------------

Sensores:
  TPS (3 fios):
    Marrom:     5V          → SCG-ECU 5V
    Preto:      GND         → SCG-ECU GND
    Cinza:      Sinal       → SCG-ECU PA0

  MAP freq (3 fios):
    Upgrade:    GM 3-bar analog
    Vermelho:   5V          → SCG-ECU 5V
    Preto:      GND         → SCG-ECU GND
    Verde:      Sinal       → SCG-ECU PA1

  CLT (2 fios):
    Azul:       Sinal       → SCG-ECU PA2
    Preto:      GND         → SCG-ECU GND

  IAT (2 fios):
    Verde:      Sinal       → SCG-ECU PA3
    Preto:      GND         → SCG-ECU GND

  Hall Distribuidor (3 fios):
    Vermelho:   5V          → SCG-ECU 5V
    Preto:      GND         → SCG-ECU GND
    Branco:     Sinal       → SCG-ECU PB0

Atuadores:
  Injetores (NOVO - 4x):
    Positivo:   12V chaveado → Relé principal
    Negativo:   Ground       → SCG-ECU PC0-PC3 (low-side)

  Bobina Ignição:
    Pino 15:    12V          → Bateria/chave
    Pino 1:     Trigger      → SCG-ECU PD0 (low-side)

Alimentação:
  SCG-ECU:
    VCC:        12V          → Bateria (fusível 15A)
    GND:        Chassis      → Múltiplos pontos
    5V:         Sensores     → Regulador interno
```

### 8.3 Diagrama Completo (ASCII)

```
                    VW GOL 1994 AP 1.8 → SCG-ECU 2.0
                    =====================================

                           ┌──────────────┐
                           │  SCG-ECU 2.0 │
                           │ STM32F407VGT6│
                           └──────────────┘
                                  │
        ┌─────────────────────────┼─────────────────────────┐
        │                         │                         │
   SENSORES                   ATUADORES                ALIMENTAÇÃO
   ========                   =========                ============
        │                         │                         │
    ┌───┴───┐               ┌─────┴─────┐            ┌──────┴──────┐
    │ TPS   │               │ Injetores │            │ Bateria 12V │
    │ MAP   │               │ Bobinas   │            │ GND Chassis │
    │ CLT   │               │ Idle Ctrl │            │ 5V Sensores │
    │ IAT   │               │ Boost PWM │            └─────────────┘
    │ O2    │               │ Fan PWM   │
    │ Hall  │               │ Fuel Pump │
    │ Knock │               └───────────┘
    │ Oil P │
    └───────┘
```

### 8.4 Bill of Materials (BOM)

**Componentes Adicionais Necessários (Preços Mercado Brasileiro 2025)**

---

#### 8.4.1 CONFIGURAÇÃO ASPIRADA BASE

**Sistema Injeção:**
```
✅ 4x Injetores 280cc (Bosch EV1)         R$ 400
✅ 4x Conectores EV1                      R$ 60
✅ 1x Flauta combustível (fuel rail)      R$ 150
✅ 1x Regulador pressão 3 bar             R$ 200
✅ 1x Bomba combustível 255 L/h           R$ 350
✅ 1x Filtro combustível high-flow        R$ 80
```

**Sensores:**
```
✅ 1x Sensor wideband LSU 4.9 + ctrl      R$ 1,000
✅ 1x Sensor MAP 3-bar (GM)               R$ 120
✅ 1x Sensor CLT (NTC 2490Ω)              R$ 45
✅ 1x Sensor IAT (NTC 2490Ω)              R$ 45
✅ 1x Sensor TPS (potenciômetro)          R$ 80
```

**Ignição:**
```
✅ 1x Bobina ignição (distribuidor)       R$ 150
✅ 1x Módulo ignição (se necessário)      R$ 120
```

**Opcional - Upgrade Decoder:**
```
⚙️ 1x Roda fônica 60-2 kit                R$ 250
⚙️ 1x Sensor Hall/VR                      R$ 120
⚙️ Bracket montagem                       R$ 80
```

**Fiação e Conectores:**
```
✅ Chicote injeção (fabricar)             R$ 300
✅ Conectores diversos (50x)              R$ 150
✅ Terminais, heat shrink                 R$ 100
✅ Fio 1.5mm² (50m)                       R$ 150
```

**SUBTOTAL ASPIRADO BASE:**             **R$ 3,350**

---

#### 8.4.2 TURBO 0.5 BAR (ADICIONAR AO ASPIRADO)

**Sistema Turbo:**
```
✅ 1x Turbo Garrett GT2860RS              R$ 4,500
✅ 1x Wastegate externa 38mm TiAL         R$ 1,200
✅ 1x Intercooler FMIC 450x300x76         R$ 1,500
✅ 1x Solenoid boost 3-port MAC           R$ 280
✅ 1x BOV/Válvula alívio TiAL             R$ 650
✅ 1x Downpipe 3" + flex                  R$ 800
✅ 1x Coletor turbo (manifold)            R$ 1,200
```

**Upgrade Fuel System:**
```
✅ 4x Injetores 440cc upgrade             R$ 650
✅ 1x Bomba Walbro GSS342 255L/h          R$ 550
✅ 1x Regulador FPR boost-ref             R$ 380
✅ 1x Filtro combustível AN-6             R$ 120
```

**Upgrade Ignição:**
```
✅ 4x Bobinas coil-on-plug GM LS1         R$ 680
✅ 4x Conectores bobinas                  R$ 120
✅ Bracket montagem COP                   R$ 150
```

**Sensores Críticos:**
```
✅ 2x Sensores knock Bosch 0261231046     R$ 320
✅ 1x Sensor pressão óleo VDO 360-004    R$ 220
✅ 2x Sensores EGT tipo K (prioritários)  R$ 400
✅ 2x Amplificadores MAX31855            R$ 180
```

**Sistema Óleo (Catch Can):**
```
✅ 1x Oil catch can NPL dual              R$ 450
✅ Mangueiras AN-10 (3m)                  R$ 180
✅ Conexões AN-10 (6x)                    R$ 150
```

**Tubulação e Abraçadeiras:**
```
✅ Tubos alumínio 2.5" (3m)               R$ 450
✅ Curvas/reduções silicone               R$ 350
✅ Abraçadeiras T-bolt (10x)              R$ 200
✅ Mangueira óleo turbo AN-4 (2m)         R$ 280
```

**Preparação Motor Básica:**
```
✅ 1x Junta cabeçote MLS Cometic 1.5mm   R$ 850
✅ 1x Kit parafusos ARP cabeçote          R$ 1,100
✅ 1x Retentores válvulas Viton           R$ 180
✅ 1x Junta coletor escape copper         R$ 220
```

**SUBTOTAL TURBO 0.5 BAR:**              **R$ 16,410**

---

#### 8.4.3 TURBO 0.8 BAR (ADICIONAR AO 0.5 BAR)

**Upgrade Sensores:**
```
✅ +2x Sensores EGT tipo K (total 4)      R$ 400
✅ +2x Amplificadores MAX31855            R$ 180
✅ +2x Sensores knock (total 4)           R$ 320
```

**Upgrade Fuel:**
```
⚙️ Injetores 550cc (substituir 440cc)     R$ 800
   OU manter 440cc e aumentar pressão     R$ 0
```

**Preparação Motor Intermediária:**
```
⚙️ 1x Comando SPA Turbo 268° (LSA 112°)  R$ 1,500
   Instalação + regulagem                R$ 800
✅ 1x Oil catch can upgrade APR           R$ 650
   (substituir NPL - vender usado)
✅ 1x Radiador óleo Setrab 10 rows        R$ 850
✅ Linhas óleo AN-10 (2m)                 R$ 240
```

**Sistema Proteção:**
```
✅ 1x Sensor fuel pressure 0-7 bar        R$ 280
✅ 1x Relé proteção óleo programável      R$ 180
```

**SUBTOTAL ADICIONAL 0.8 BAR:**          **R$ 6,200**

---

#### 8.4.4 TURBO 1.0 BAR (ADICIONAR AO 0.8 BAR)

**ATENÇÃO: Motor preparado profissionalmente obrigatório!**

**Preparação Motor Completa:**
```
✅ 4x Pistões forjados SPA Turbo 8.5:1    R$ 2,800
✅ 4x Bielas forjadas SPA I-beam          R$ 3,200
✅ 1x Kit anéis Total Seal                R$ 650
✅ 1x Kit bronzinas ACL Race              R$ 550
✅ 1x Bomba óleo reforçada                R$ 850
✅ Usinagem + balanceamento               R$ 1,800
✅ Mão de obra montagem motor             R$ 2,500
```

**Upgrade Fuel Obrigatório:**
```
✅ 4x Injetores 660cc Bosch EV14          R$ 1,100
✅ 1x Bomba Walbro F90000285 450L/h       R$ 950
✅ 1x Regulador Aeromotive A1000-6        R$ 1,200
```

**Upgrade Turbo:**
```
⚙️ Turbo Garrett GTX2867R (upgrade)       R$ 6,500
   OU manter GT2860RS                     R$ 0
```

**Sistema Proteção Avançado:**
```
✅ 1x Display AEM CD-7 ou similar         R$ 3,500
✅ 1x Sistema datalogging profissional    R$ 1,200
✅ 4x Sensores EGT já instalados          R$ 0
✅ 1x Sensor fuel pressure já instalado   R$ 0
```

**Reforços Adicionais:**
```
✅ 1x Embreagem cerâmica 6 pastilhas      R$ 2,200
✅ 1x Volante motor aliviado              R$ 1,500
✅ 1x Diferencial autoblocante (LSD)      R$ 3,800
```

**SUBTOTAL ADICIONAL 1.0 BAR:**          **R$ 34,850**

---

#### 8.4.5 AR CONDICIONADO (ADICIONAR A QUALQUER CONFIG)

**Componentes AC:**
```
✅ 1x Compressor Delphi CVC (original)    R$ 850
✅ 1x Condensador AC                      R$ 450
✅ 1x Evaporador AC                       R$ 380
✅ 1x Filtro secador                      R$ 120
✅ 1x Válvula expansão termostática       R$ 180
✅ 1x Sensor pressão AC (0-300 psi)       R$ 150
✅ 1x Relé AC 30A                         R$ 45
```

**Sistema Elétrico AC:**
```
✅ 1x Interruptor painel AC               R$ 80
✅ Chicote AC (fabricar)                  R$ 150
✅ Fusível 30A + porta fusível            R$ 35
```

**Tubulação AC:**
```
✅ Mangueiras AC alta pressão (conjunto)  R$ 450
✅ Conexões AC (8x)                       R$ 180
✅ Abraçadeiras AC (6x)                   R$ 60
```

**Instalação e Carga:**
```
✅ Gás R134a (800g)                       R$ 180
✅ Óleo PAG46 (250ml)                     R$ 85
✅ Vácuo + carga profissional             R$ 250
```

**Turbo: Upgrade Obrigatório:**
```
⚠️ Intercooler maior 600x300x76           R$ 1,800
   (substituir 450x300 - vender usado)
⚠️ Tubulação intercooler nova             R$ 350
⚠️ Fan elétrica adicional 12"             R$ 280
```

**SUBTOTAL AC ASPIRADO:**                **R$ 3,645**
**SUBTOTAL AC TURBO:**                   **R$ 6,075**

---

#### 8.4.6 CÂMBIO SEQUENCIAL + PADDLE SHIFT (OPCIONAL)

**Sistema Pneumático:**
```
✅ 1x Atuador pneumático push/pull linear R$ 1,800
✅ 1x Válvula solenóide 3/2 vias dupla    R$ 650
✅ 1x Tanque ar comprimido 2L             R$ 350
✅ 1x Compressor 12V 150psi               R$ 550
✅ 1x Pressostato 6-8 bar                 R$ 180
✅ Mangueiras pneumáticas 6mm (5m)        R$ 150
✅ Conexões push-fit (12x)                R$ 120
```

**Controles Paddle:**
```
✅ 2x Paddle switches (UP/DOWN)           R$ 280
✅ 1x Suporte coluna direção (fabricar)   R$ 250
✅ Chicote paddle (fabricar)              R$ 150
```

**Instalação Mecânica:**
```
✅ Bracket atuador câmbio (fabricar)      R$ 350
✅ Linkage atuador → alavanca             R$ 280
✅ Hardware fixação (parafusos, porcas)   R$ 120
```

**Configuração SCG-ECU:**
```
✅ Calibração flat-shift (tunerstudio)    R$ 0
✅ Calibração paddle timing               R$ 0
✅ Datalog + ajustes (1 dia)              R$ 500
```

**Instalação Profissional:**
```
⚠️ Mão de obra instalação completa        R$ 1,200
⚠️ Testes + ajustes mecânicos             R$ 500
```

**SUBTOTAL SEQUENCIAL (DIY):**           **R$ 4,930**
**SUBTOTAL SEQUENCIAL (INSTALADO):**     **R$ 6,630**

---

#### 8.4.7 FERRAMENTAS E EQUIPAMENTOS NECESSÁRIOS

**Ferramentas Básicas (se não tiver):**
```
⚙️ Multímetro automotivo                  R$ 180
⚙️ Timing light estroboscópica            R$ 250
⚙️ Ferro de solda 60W + estação           R$ 280
⚙️ Crimp tool terminais automotivos       R$ 150
⚙️ Jogo chaves combinadas (milímetro)     R$ 350
⚙️ Jogo soquetes 1/2" (10-32mm)           R$ 280
⚙️ Torquímetro 10-150 Nm                  R$ 350
```

**Equipamentos Diagnóstico:**
```
⚙️ Laptop/PC (TunerStudio)                R$ 2,500
⚙️ Cabo USB-Serial FTDI                   R$ 85
⚙️ Software TunerStudio MS                R$ 350
⚙️ MegaLogViewer HD                       R$ 250
```

**Equipamentos Teste Bancada:**
```
⚙️ Fonte 12V 20A (testes)                 R$ 280
⚙️ LEDs teste injetores (4x)              R$ 60
⚙️ Simulador RPM (opcional)               R$ 450
```

**SUBTOTAL FERRAMENTAS:**                **R$ 5,815**

---

#### 8.4.8 CUSTOS INSTALAÇÃO PROFISSIONAL (OPCIONAL)

**Mão de Obra Aspirado:**
```
⚙️ Instalação mecânica injeção            R$ 800
⚙️ Instalação elétrica completa           R$ 1,200
⚙️ Configuração SCG-ECU base              R$ 500
⚙️ Primeira partida + ajustes             R$ 350
⚙️ Street tuning (2 horas)                R$ 600
⚙️ Dyno tuning (3 horas)                  R$ 1,200
```

**Mão de Obra Turbo:**
```
⚙️ Preparação motor (rebaixar CR)         R$ 1,500
⚙️ Instalação sistema turbo completo      R$ 2,500
⚙️ Instalação sensores EGT/knock/oil      R$ 800
⚙️ Configuração proteções turbo           R$ 500
⚙️ Dyno tuning turbo progressivo (6h)     R$ 3,000
```

**SUBTOTAL MÃO DE OBRA ASPIRADO:**       **R$ 4,650**
**SUBTOTAL MÃO DE OBRA TURBO:**          **R$ 8,300**

---

### 8.4.9 RESUMO FINANCEIRO - TODAS CONFIGURAÇÕES

#### Opção 1: ASPIRADO PURO
```
Base aspirado:                            R$ 3,350
Ferramentas (se necessário):              R$ 5,815
Mão de obra (opcional):                   R$ 4,650
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TOTAL DIY:                                R$ 3,350
TOTAL COM FERRAMENTAS:                    R$ 9,165
TOTAL INSTALADO:                          R$ 13,815
```

#### Opção 2: ASPIRADO + AR CONDICIONADO
```
Base aspirado:                            R$ 3,350
Ar condicionado:                          R$ 3,645
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TOTAL:                                    R$ 6,995
```

#### Opção 3: ASPIRADO + SEQUENCIAL
```
Base aspirado:                            R$ 3,350
Câmbio sequencial:                        R$ 4,930
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TOTAL DIY:                                R$ 8,280
TOTAL INSTALADO:                          R$ 9,980
```

#### Opção 4: TURBO 0.5 BAR (CONSERVADOR)
```
Base aspirado:                            R$ 3,350
Turbo 0.5 bar:                            R$ 16,410
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TOTAL DIY:                                R$ 19,760
TOTAL INSTALADO:                          R$ 32,710
```

#### Opção 5: TURBO 0.8 BAR (INTERMEDIÁRIO)
```
Base aspirado:                            R$ 3,350
Turbo 0.5 bar:                            R$ 16,410
Turbo 0.8 bar upgrade:                    R$ 6,200
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TOTAL DIY:                                R$ 25,960
TOTAL INSTALADO:                          R$ 38,910
```

#### Opção 6: TURBO 1.0 BAR (PROFISSIONAL)
```
Base aspirado:                            R$ 3,350
Turbo 0.5 bar:                            R$ 16,410
Turbo 0.8 bar upgrade:                    R$ 6,200
Turbo 1.0 bar upgrade:                    R$ 34,850
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TOTAL DIY:                                R$ 60,810
TOTAL INSTALADO:                          R$ 73,760
```

#### Opção 7: TURBO 0.5 BAR + AC + SEQUENCIAL (FULL FEATURED)
```
Base aspirado:                            R$ 3,350
Turbo 0.5 bar:                            R$ 16,410
Ar condicionado turbo:                    R$ 6,075
Câmbio sequencial:                        R$ 4,930
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TOTAL DIY:                                R$ 30,765
TOTAL INSTALADO:                          R$ 45,345
```

#### Opção 8: TURBO 1.0 BAR + AC + SEQUENCIAL (ULTIMATE)
```
Base aspirado:                            R$ 3,350
Turbo 0.5 bar:                            R$ 16,410
Turbo 0.8 bar upgrade:                    R$ 6,200
Turbo 1.0 bar upgrade:                    R$ 34,850
Ar condicionado turbo:                    R$ 6,075
Câmbio sequencial:                        R$ 6,630
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TOTAL INSTALADO:                          R$ 73,515
COM FERRAMENTAS:                          R$ 79,330
COM MÃO DE OBRA TOTAL:                    R$ 92,630
```

---

### 8.4.10 RECOMENDAÇÕES COMPRA FASEADA

**Para Iniciantes - Caminho Progressivo:**

```
FASE 1 - Base (R$ 3,350):
  ✅ Sistema aspirado completo
  ✅ Aprender SCG-ECU
  ✅ Desenvolver habilidades tuning
  ⏱️ Duração: 3-4 meses

FASE 2 - Conforto (R$ 3,645):
  ✅ Adicionar ar condicionado
  ✅ Melhorar usabilidade diária
  ⏱️ Duração: 1 mês

FASE 3 - Performance Básica (R$ 16,410):
  ✅ Turbo 0.5 bar conservador
  ✅ +35 cv (~132 cv total)
  ✅ Confiabilidade alta
  ⏱️ Duração: 3-4 meses

FASE 4 - Racing Features (R$ 4,930):
  ✅ Câmbio sequencial + paddle
  ✅ Experiência track day
  ⏱️ Duração: 2 meses

FASE 5 - Performance Avançada (R$ 6,200):
  ✅ Turbo 0.8 bar
  ✅ +55 cv (~152 cv total)
  ⏱️ Duração: 2-3 meses

TOTAL INVESTIMENTO PROGRESSIVO: R$ 34,555
TEMPO TOTAL: ~15-17 meses
```

**Para Experientes - Caminho Direto:**

```
OPÇÃO A - Turbo Street (R$ 25,960):
  ✅ Aspirado + Turbo 0.8 bar direto
  ✅ ~152 cv
  ⏱️ Duração: 6-8 semanas

OPÇÃO B - Turbo Track (R$ 30,765):
  ✅ Aspirado + Turbo 0.5 bar + AC + Sequencial
  ✅ ~132 cv, conforto + racing
  ⏱️ Duração: 8-10 semanas

OPÇÃO C - Ultimate Build (R$ 73,515):
  ✅ Turbo 1.0 bar + AC + Sequencial
  ✅ ~185 cv, preparação completa
  ⏱️ Duração: 16-20 semanas
  ⚠️ Motor preparado profissionalmente obrigatório!
```

---

### 8.4.11 FORNECEDORES RECOMENDADOS (BRASIL)

**Injeção e Sensores:**
```
• Injector Dynamics Brasil        (injetores importados)
• Tury                             (sensores wideband)
• MTE-Thomson                      (sensores temperatura)
• Magneti Marelli                  (sensores MAP, TPS)
```

**Turbo e Componentes:**
```
• Garrett Brasil                   (turbos originais)
• TiAL Sport                       (BOV, wastegate)
• Turbonetics Brasil               (turbos, wastegate)
• Full Turbo                       (kits completos)
```

**Preparação Motor:**
```
• SPA Turbo                        (pistões, bielas, comando)
• Pauter                           (bielas forjadas)
• Wiseco Brasil                    (pistões)
• ARP Brasil                       (parafusos)
```

**Fuel System:**
```
• Aeromotive                       (reguladores, bombas)
• Walbro                           (bombas)
• Bosch                            (injetores, sensores)
```

**Intercooler e Tubulação:**
```
• PWR Brasil                       (intercoolers)
• Forge Motorsport Brasil          (tubulação silicone)
• Vibrant Performance              (tubos alumínio)
```

**AC e Refrigeração:**
```
• Denso                            (compressores AC)
• Delphi                           (componentes AC)
• Setrab                           (radiadores óleo)
```

**Câmbio Sequencial:**
```
• Geartronics Brasil               (atuadores pneumáticos)
• Quaife                           (LSD, sequencial)
• Pfitzner Performance             (sistemas completos)
```

---

### 8.4.12 NOTAS IMPORTANTES

**⚠️ AVISOS CRÍTICOS:**

1. **Preços são estimativas 2025** - Podem variar ±15% dependendo:
   - Fornecedor escolhido
   - Região do Brasil
   - Câmbio (componentes importados)
   - Promoções/descontos quantidade

2. **Turbo 1.0 bar REQUER motor preparado:**
   - Pistões forjados obrigatórios
   - Bielas forjadas obrigatórias
   - Montagem profissional obrigatória
   - NÃO tentar em motor stock!

3. **Mão de obra varia por região:**
   - SP/RJ: preços 20-30% mais altos
   - Sul: preços médios mostrados
   - Norte/Nordeste: preços 10-20% menores
   - Buscar oficinas especializadas em turbo!

4. **Ferramentas:** Se já possui ferramentas básicas, economiza ~R$ 5,800

5. **Imprevistos:** Sempre adicionar 15-20% para:
   - Componentes adicionais descobertos durante instalação
   - Substituição peças desgastadas motor
   - Upgrades decididos durante projeto

**✅ DICAS ECONOMIA:**

- Comprar conjunto completo: desconto 10-15%
- Grupos compra coletiva AP Turbo: desconto adicional
- Usar componentes GM LS1 (bobinas): mais barato que específico
- Fabricar chicotes próprios: economia R$ 500-800
- DIY instalação: economia R$ 4,650 (aspirado) ou R$ 8,300 (turbo)

---

**FIM DA SEÇÃO 8.4 BOM**

---

## 9. ROADMAP DE IMPLEMENTAÇÃO

### 9.1 FASE 1: Preparação (2 semanas)

**Semana 1-2: Estudo e Compras**
```
□ Estudar documentação SCG-ECU 2.0
□ Estudar TunerStudio software
□ Comprar componentes básicos (sensores, injetores)
□ Preparar bancada de testes
□ Instalar TunerStudio + firmware SCG-ECU
□ Configurar comunicação USB
```

### 9.2 FASE 2: Instalação Aspirada (3-4 semanas)

**Semana 3: Mechanical**
```
□ Remover injeção monoponto original
□ Instalar flauta combustível (fuel rail)
□ Montar 4x injetores 280cc
□ Instalar regulador pressão 3 bar
□ Trocar bomba combustível 255 L/h
□ Instalar sensor wideband O2
□ Upgrade MAP sensor (GM 3-bar)
□ (Opcional) Instalar roda fônica 60-2
```

**Semana 4: Elétrica**
```
□ Fabricar chicote injeção
□ Conectar sensores → SCG-ECU
□ Conectar injetores → SCG-ECU
□ Conectar ignição → SCG-ECU
□ Instalar display/gauges
□ Verificar continuidade/curtos
□ Testar alimentação 12V/5V
```

**Semana 5: Configuração Base**
```
□ Carregar config base AP 1.8
□ Calibrar TPS (closed/WOT)
□ Calibrar MAP sensor
□ Calibrar CLT/IAT
□ Calibrar wideband O2
□ Configurar decoder (Hall ou 60-2)
□ Configurar injetores (dead time, flow)
□ Configurar ignição (dwell)
```

**Semana 6: Primeira Partida**
```
□ Verificar sincronismo (timing light)
□ Ajustar TDC (se roda fônica)
□ Primeira partida (rich!)
□ Verificar idle estável
□ Ajustar VE table idle
□ Ajustar advance idle
□ Teste estrada (conservador)
□ Data logging completo
```

### 9.3 FASE 3: Tuning Aspirado (2-3 semanas)

**Semana 7-8: Street Tuning**
```
□ VE table tuning (wideband)
  - Idle: lambda 1.0
  - Cruise: lambda 1.05
  - Aceleração: lambda 0.90
  - WOT: lambda 0.85

□ Ignition advance tuning
  - Idle: 9° (verificar estabilidade)
  - Cruise: 20° (economia)
  - WOT: 24-26° (potência)

□ Fuel corrections
  - WUE (motor frio)
  - ASE (pós-partida)
  - AE (aceleração)
  - Battery voltage

□ Ignition corrections
  - CLT (motor frio)
  - IAT (ar quente)
```

**Semana 9: Dyno (Recomendado)**
```
□ Dyno baseline (power/torque)
□ Otimizar VE table WOT
□ Otimizar advance WOT
□ Verificar AFR todos RPMs
□ Verificar timing todos RPMs
□ Ajuste fino potência
□ Imprimir relatório dyno
```

### 9.4 FASE 4: Validação Aspirado (2 semanas)

**Semana 10-11: Testes Reais**
```
□ 500 km rodagem mista
□ Verificar consumo combustível
□ Verificar temperatura motor
□ Verificar partidas frias
□ Verificar idle em trânsito
□ Data logging contínuo
□ Identificar issues
□ Ajustes finais
```

### 9.5 FASE 5: Preparação Turbo (4-6 semanas)

**ATENÇÃO: Só iniciar após aspirado 100% funcional!**

**Semana 12-14: Preparação Motor**
```
□ Rebaixar taxa compressão (8.5:1)
  - Junta cabeçote +1.5mm MLS
  - OU pistões forjados baixa CR

□ Reforços mecânicos
  - Parafusos ARP cabeçote
  - Retentores válvula alta temp
  - Bomba óleo reforçada
  - Juntas novas todas

□ Retífica motor (opcional)
  - Pistões forjados
  - Bielas forjadas
  - Anéis Total Seal
  - Bronzinas ACL
```

**Semana 15-16: Instalação Turbo**
```
□ Coletor turbo (manifold)
□ Turbo Garrett GT2860RS
□ Wastegate externa 38mm
□ Downpipe 3"
□ Intercooler FMIC
□ Tubulação admissão
□ BOV/válvula alívio
□ Linhas óleo turbo (feed/return)
□ Escape completo 3"
```

**Semana 17: Upgrade Fuel/Ignition**
```
□ Injetores 440cc (trocar 280cc)
□ Bomba Walbro GSS342
□ Regulador FPR boost-ref
□ 4x Bobinas coil-on-plug
□ Sensor knock (1-4x)
□ Sensor oil pressure
□ Sensor EGT (1-4x)
□ Chicote turbo
```

### 9.6 FASE 6: Tuning Turbo (3-4 semanas)

**Semana 18: Configuração Base Turbo**
```
□ Carregar config turbo AP 1.8
□ Configurar injetores 440cc
□ Configurar boost control
□ Configurar knock control
□ Configurar proteções
□ Verificar todas conexões
```

**Semana 19-20: Break-in Turbo**
```
□ Primeira partida (NA mode)
□ 500 km break-in motor rebaixado
□ Verificar vazamentos óleo
□ Verificar temperatura motor
□ 0 PSI boost (wastegate aberta)
□ Ajustar idle/cruise
□ Medir compressão cilindros
```

**Semana 21: Boost Progressivo**
```
□ Semana 1: 0.3 bar (4 psi)
  - 30 km teste
  - Verificar knock
  - Verificar AFR
  - Verificar EGT

□ Semana 2: 0.5 bar (7 psi)
  - 50 km teste
  - Tuning VE boost
  - Tuning advance retard
  - Verificar proteções

□ Semana 3: 0.8 bar (11 psi)
  - Dyno obrigatório
  - Tuning fino WOT
  - Otimizar potência
  - Verificar limites

□ Semana 4: 1.0 bar (14 psi)
  - Dyno final
  - Verificar knock contínuo
  - Verificar EGT <900°C
  - Tuning conservador
  - Medir potência final
```

### 9.7 FASE 7: Validação Turbo (4 semanas)

**Semana 22-25:**
```
□ 1000 km rodagem turbo
□ Data logging 100% tempo
□ Monitorar:
  - Knock events
  - EGT máximos
  - Oil pressure
  - Boost spikes
  - AFR desvios

□ Ajustes contínuos
□ Documentar setup final
□ Imprimir dyno sheets
□ Celebrar! 🎉
```

---

## 10. RECURSOS SCG-ECU DISPONÍVEIS

### 10.1 Recursos Implementados (100%)

**✅ SISTEMA DE INJEÇÃO:**
```
✓ Sequential injection (8 canais)
✓ Semi-sequential (4 canais)
✓ Paired injection (2 grupos)
✓ Fuel staging (primary + secondary)
✓ Injector dead time compensation
✓ Battery voltage compensation
✓ Per-cylinder fuel trim
```

**✅ SISTEMA DE IGNIÇÃO:**
```
✓ Sequential ignition (8 canais)
✓ Wasted spark (4 canais)
✓ Distributor mode (1 canal)
✓ Dwell control (voltage compensated)
✓ Per-tooth timing
✓ Multi-spark (low RPM)
```

**✅ DECODERS (28 suportados):**
```
✓ Missing tooth (36-1, 60-2, 12-1, etc)
✓ Dual wheel
✓ Basic distributor (Hall)
✓ GM 7X, 24X
✓ Honda D17, J32
✓ Nissan 360
✓ Subaru 6/7
✓ E mais 20 padrões
```

**✅ CORREÇÕES COMBUSTÍVEL:**
```
✓ Warmup enrichment (WUE)
✓ Afterstart enrichment (ASE)
✓ Acceleration enrichment (AE - TPS/MAP)
✓ Deceleration enleanment (DE)
✓ Air density (IAT compensation)
✓ Barometric compensation
✓ Battery voltage compensation
✓ Closed loop O2 (lambda target)
✓ Flex fuel compensation (com sensor)
✓ DFCO (decel fuel cut-off)
```

**✅ CORREÇÕES IGNIÇÃO:**
```
✓ Coolant temperature (CLT)
✓ Intake air temperature (IAT)
✓ Flex fuel timing
✓ WMI timing (water/meth injection)
✓ Idle advance
✓ Soft rev limiter
✓ Soft launch control
✓ Soft flat shift
✓ Knock timing retard
```

**✅ AUXILIARES (10 subsistemas):**
```
✓ Boost control (closed-loop PID)
  - 3D table RPM x TPS
  - PWM valve control
  - Overboost protection

✓ VVT control (Variable Valve Timing)
  - Cam advance/retard
  - Oil pressure compensation

✓ Idle control
  - Stepper motor
  - PWM valve
  - PID closed-loop

✓ Fan control
  - ON/OFF thermostatic
  - PWM variable speed

✓ Launch control (2-step)
  - RPM target
  - Spark retard (anti-lag)
  - Boost building

✓ Flat shift (no-lift shift)
  - Clutch switch
  - Momentary ignition cut
  - Boost maintain

✓ Fuel pump control
  - Prime pulse
  - Running control
  - Pressure sensor feedback

✓ Nitrous control
  - 2-stage progressive
  - RPM/TPS windows
  - Fuel enrichment
  - Timing retard

✓ ASE (After Start Enrichment)
  - Time-based decay
  - CLT compensation

✓ WMI (Water/Methanol Injection)
  - Boost threshold
  - Progressive injection
  - Timing advance
```

**✅ PROTEÇÕES:**
```
✓ Rev limiter (soft + hard)
✓ Overboost cut
✓ Knock retard
✓ CLT overheat protection
✓ Oil pressure protection (com sensor)
✓ Flood clear mode
✓ Battery low voltage
✓ EGT protection (com sensor)
```

**✅ SENSORES SUPORTADOS:**
```
✓ TPS (0-5V potentiometer)
✓ MAP (analog 0-5V ou frequency)
✓ CLT (NTC thermistor)
✓ IAT (NTC thermistor)
✓ O2 narrowband (0-1V)
✓ O2 wideband (0-5V linear)
✓ Flex fuel sensor (frequency)
✓ Knock sensor (analog piezo)
✓ Oil pressure (0-5V analog)
✓ Fuel pressure (0-5V analog)
✓ EGT (K-type thermocouple)
✓ Battery voltage (0-5V divider)
✓ VSS (Vehicle Speed - frequency)
✓ Cam sensor (Hall)
✓ Crank sensor (Hall/VR)
```

**✅ COMUNICAÇÃO:**
```
✓ TunerStudio (USB/Serial)
✓ Real-time dashboard
✓ Live tuning
✓ Data logging SD card
✓ CAN bus output
✓ Composite log
```

### 10.2 O Que a SCG-ECU TEM que o Original NÃO TEM

**COMPARAÇÃO DETALHADA:**

| Recurso | FIC EEC-IV (Original) | SCG-ECU 2.0 |
|---------|----------------------|-------------|
| **INJEÇÃO** |
| Canais | 1 (monoponto) | 8 independentes ✅ |
| Sequential | Não | Sim ✅ |
| Staging | Não | Sim ✅ |
| Trim individual | Não | Sim ✅ |
| **IGNIÇÃO** |
| Canais | 1 (distribuidor) | 8 independentes ✅ |
| Sequential | Não | Sim ✅ |
| Per-tooth timing | Não | Sim ✅ |
| Multi-spark | Não | Sim ✅ |
| **BOOST** |
| Controle ativo | Não | Sim (PID) ✅ |
| Target 3D table | Não | Sim ✅ |
| Overboost protect | Não | Sim ✅ |
| **KNOCK** |
| Detecção | Não | Sim ✅ |
| Auto retard | Não | Sim ✅ |
| Per-cylinder | Não | Sim (4x) ✅ |
| **LAUNCH/FLAT SHIFT** |
| Launch control | Não | Sim ✅ |
| Flat shift | Não | Sim ✅ |
| 2-step rev | Não | Sim ✅ |
| Anti-lag | Não | Sim ✅ |
| **VVT** |
| Controle | Não | Sim ✅ |
| Cam advance | Não | Sim ✅ |
| **FLEX FUEL** |
| Sensor | Não | Sim ✅ |
| Auto adjust | Não | Sim ✅ |
| **DATA LOGGING** |
| SD card | Não | Sim ✅ |
| CAN bus | Não | Sim ✅ |
| Real-time | Não | Sim ✅ |
| **TUNING** |
| Software | Não (chip) | TunerStudio ✅ |
| Live edit | Não | Sim ✅ |
| 16x16 tables | Não | Sim ✅ |
| **PROTEÇÕES** |
| EGT monitor | Não | Sim ✅ |
| Oil pressure | Não | Sim ✅ |
| Overboost | Não | Sim ✅ |
| Multi-level | Não | Sim ✅ |

### 10.3 Recursos Futuros (RTOS Migration)

**PLANEJADO - Fase 3 (após testes HIL):**

```
FreeRTOS Integration:
  □ Deterministic timing (<10µs jitter)
  □ Task priorities
  □ Memory protection
  □ Stack monitoring
  □ Watchdog integration
  □ ISO 26262 prep (automotive safety)
```

---

## CONCLUSÃO

### Resumo do Projeto

**ASPIRADO (Fase 1):**
- ✅ Todos os recursos SCG-ECU funcionais
- ✅ Injeção sequencial 4x injetores
- ✅ Mapeamento 16x16 completo
- ✅ Proteções avançadas
- ✅ Data logging completo
- 💰 Investimento: ~R$ 3,000
- ⏱️ Tempo: 8-10 semanas
- 🎯 Resultado: Motor mais eficiente, econômico e confiável

**TURBO (Fase 2):**
- ✅ Boost control integrado (PID)
- ✅ Knock control multi-cilindro
- ✅ Launch + Flat shift
- ✅ Proteções turbo (EGT, oil, overboost)
- ✅ 150-180 cv confiáveis
- 💰 Investimento: +R$ 12,880
- ⏱️ Tempo: +12-16 semanas
- 🎯 Resultado: Performance dobrada, confiabilidade mantida

### Próximos Passos

1. ✅ **Estudar este documento completo**
2. ✅ **Decidir: Aspirado ou Turbo direto?**
3. ⏸️ **Comprar componentes (ver BOM)**
4. ⏸️ **Preparar bancada de testes**
5. ⏸️ **Seguir roadmap implementação**

### Suporte e Documentação

- 📖 **Este documento:** Referência completa
- 💻 **TunerStudio:** Software tuning
- 🔧 **SCG-ECU docs:** Documentação técnica
- 🌐 **Speeduino forum:** Comunidade
- 📧 **Contato:** (seu email/forum)

---

**FIM DA DOCUMENTAÇÃO COMPLETA**

**Versão:** 1.0
**Data:** 01/11/2025
**Autor:** Claude + Guiito
**Projeto:** SCG-ECU 2.0 - VW Gol AP 1.8

---

**NOTA IMPORTANTE:**

Este documento é baseado em especificações técnicas reais pesquisadas do VW Gol Quadrado AP 1.8 MI (1994) e nas capacidades documentadas do projeto SCG-ECU 2.0 (Speeduino STM32F407 refatorado).

Todos os valores de tabelas (VE, Advance, AFR) são **valores base conservadores** e devem ser ajustados em dyno ou street tuning com wideband O2 para o motor específico.

**SEMPRE** fazer tuning progressivo e monitorar:
- ✅ AFR (wideband)
- ✅ Knock (sensor)
- ✅ EGT (turbo)
- ✅ Oil pressure (turbo)
- ✅ CLT
- ✅ Data logging contínuo

