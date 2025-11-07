# BMW M54B30 - SENSORES E ATUADORES

**Motor:** BMW M54B30 3.0L inline-6
**Aplicação:** BMW 330i E46 (2000-2006)
**ECU Original:** Siemens MS43
**Data:** 2025-11-07

---

## 🌡️ SENSORES DE TEMPERATURA

### CLT - Coolant Temperature Sensor

**Localização:** Cabeçote, lado esquerdo frontal
**Part Number:** BMW 13621433077
**Tipo:** NTC thermistor (negative temperature coefficient)

**Especificações Elétricas:**
```
Resistência @ 6°C:    ~4500 Ω
Resistência @ 20°C:   ~2500 Ω
Resistência @ 42°C:   ~1100 Ω
Resistência @ 80°C:   ~300 Ω
Resistência @ 86°C:   ~270 Ω
Resistência @ 100°C:  ~180 Ω

Voltagem Pull-up:     5V (via resistor 2.2kΩ)
Range de Voltagem:    0.05V - 5.05V
```

**Curva de Resistência:**
```
Temp (°C)  |  Resistência (Ω)  |  Voltagem (V)
-----------|-------------------|---------------
  -40      |      100,000      |      4.98
  -20      |       25,000      |      4.82
    0      |        9,500      |      4.49
   20      |        2,500      |      3.46
   40      |        1,150      |      2.64
   60      |          580      |      1.84
   80      |          300      |      1.20
  100      |          180      |      0.78
  120      |          105      |      0.48
```

**⚠️ SCG-ECU:** Totalmente compatível - curva NTC configurável no TunerStudio

---

### IAT - Intake Air Temperature Sensor

**Localização (2001-03/2003):** Entre runners 3 e 4 (topo do motor)
**Localização (04/2003+):** Integrado no MAF (conector 6-pin)
**Part Number:** BMW 13621747155 (standalone)
**Tipo:** NTC thermistor (idêntico ao CLT)

**Especificações Elétricas:**
```
Resistência @ 0°C:    ~9500 Ω
Resistência @ 20°C:   ~2500 Ω
Resistência @ 40°C:   ~1150 Ω
Resistência @ 60°C:   ~580 Ω

Voltagem Pull-up:     5V (via resistor 2.2kΩ)
Range de Voltagem:    0.05V - 5.05V
```

**⚠️ SCG-ECU:** Usar sensor standalone (não integrado no MAF, já que MAP substituirá MAF)

---

### Oil Temperature Sensor (Opcional)

**Localização:** Cárter inferior (filter housing)
**Part Number:** BMW 12617592532
**Tipo:** NTC thermistor

**Especificações:**
```
Resistência @ 20°C:   ~2500 Ω
Resistência @ 80°C:   ~300 Ω
Resistência @ 100°C:  ~180 Ω

Range de Operação:    -40°C a 150°C
```

**⚠️ SCG-ECU:** Pode usar entrada analógica auxiliar (opcional)

---

## 🔄 SENSORES DE POSIÇÃO

### CKP - Crankshaft Position Sensor

**Localização:** Lado direito do bloco, próximo ao volante
**Part Number:** BMW 12141247978
**Tipo:** Hall effect sensor (active)

**Especificações:**
```
Trigger Wheel:        60-2 (60 dentes, 2 faltando)
Gap do Sensor:        1-2 mm (nominal)
Missing Teeth Pos:    90° BTDC cilindro #1
Voltagem Operação:    5V - 12V
Sinal de Saída:       Square wave (0-5V ou 0-12V)
Frequência @ 1000RPM: ~1000 Hz
Frequência @ 6000RPM: ~6000 Hz
```

**Timing do Missing Teeth:**
```
Gap (2 dentes faltando) = 90° BTDC cyl #1
Usado para sincronização do ciclo 720°
```

**⚠️ SCG-ECU:**
- Decoder: **MISSING_TOOTH** (60-2)
- Totalmente compatível - já implementado no firmware
- Pin: PA15 (TIM2_CH1)

---

### CMP - Camshaft Position Sensors (2x)

**Quantidade:** 2 sensores (intake + exhaust)
**Part Number:** BMW 12141438082
**Tipo:** Hall effect sensor (active)

**Localização:**
```
CMP Intake:  Frente do cabeçote, lado intake
CMP Exhaust: Traseira do cabeçote, lado exhaust
```

**Especificações:**
```
Trigger Type:         1 pulse/revolution (cam revolution = 2x crank rev)
Voltagem Operação:    5V - 12V
Sinal de Saída:       Square wave (0-5V ou 0-12V)
Gap do Sensor:        1-2 mm
```

**Função:**
- Determinar qual cilindro está em fase de combustão
- Necessário para injeção/ignição sequencial
- Sincronizar VANOS

**⚠️ SCG-ECU:**
- Usar apenas **CMP Intake** como trigger secundário
- Pin: PB3 (TIM2_CH2)
- CMP Exhaust pode ser lido como entrada digital (opcional para VANOS feedback)

---

### TPS - Throttle Position Sensor

**Localização:** Corpo de borboleta eletrônico (60mm)
**Part Number:** Integrado na borboleta BMW 13547501326
**Tipo:** Dual potentiometer (redundante)

**Especificações (MS43 original):**
```
Sensores:             2x potenciômetros (TPS1 + TPS2)
Resistência Total:    ~2-5 kΩ (variável)
Voltagem TPS1:        0.5V (fechado) → 4.5V (aberto)
Voltagem TPS2:        4.5V (fechado) → 0.5V (aberto) [invertido]
Range de Voltagem:    0-5V
```

**⚠️ SCG-ECU:**
- Não suporta drive-by-wire (borboleta eletrônica)
- **Solução:** Usar borboleta mecânica com TPS único
- Opções de TPS mecânico:
  - GM TPS (3-pin): 0.5-4.5V
  - Bosch TPS: 0.5-4.5V
  - BMW E30/E36 TPS (mecânico): compatível
- Pin: PA2 (ADC1_IN2)

---

### VANOS Position Sensors (2x)

**Quantidade:** 2 sensores (intake + exhaust)
**Part Number:** Integrado nas unidades VANOS
**Tipo:** Potentiometer (analog position feedback)

**Especificações:**
```
Range de Voltagem:    0-5V
Posição Retraída:     ~0.5V (sem avanço)
Posição Avançada:     ~4.5V (máximo avanço)
Resistência:          ~5 kΩ variable
```

**⚠️ SCG-ECU:**
- Entrada analógica para feedback closed-loop
- VVT1 (Intake): PA5 (ADC1_IN5)
- VVT2 (Exhaust): PA6 (ADC1_IN6)

---

## 🌬️ SENSORES DE AR

### MAF - Mass Air Flow Sensor (ORIGINAL BMW)

**Localização:** Tubo de admissão, após filtro de ar
**Part Number:** BMW 13621432356 (Bosch 0 280 217 814)
**Tipo:** Hot-wire anemometer

**Especificações:**
```
Range de Voltagem:    0-5V
Fluxo em Idle:        ~2-4 g/s
Fluxo em WOT:         ~180-200 g/s @ 6000 RPM
Resistência Interna:  ~2.5 kΩ
Corrente Máxima:      ~15 mA
```

**Curva de Voltagem vs Massa de Ar:**
```
Voltagem (V)  |  Fluxo (g/s)
--------------|-------------
    0.5       |      0
    1.0       |      5
    1.5       |     15
    2.0       |     30
    2.5       |     50
    3.0       |     80
    3.5       |    120
    4.0       |    160
    4.5       |    200
```

**⚠️ SCG-ECU:**
- **NÃO COMPATÍVEL** - SCG-ECU usa MAP (não MAF)
- Remover sensor MAF

---

### MAP - Manifold Absolute Pressure (NOVO - NECESSÁRIO)

O E46 330i **NÃO** vem com sensor MAP de fábrica. É necessário instalar.

**Sensor Recomendado:** GM 3-Bar MAP Sensor
**Part Number:** GM 12223861 (ou equivalente ACDelco 213-796)

**Especificações:**
```
Range de Pressão:     0-3 bar (0-300 kPa, 0-44 psi)
Voltagem Operação:    5V regulated
Range de Voltagem:    0.5V (vácuo) → 4.5V (boost)
Linearidade:          ±1% FSO
```

**Curva de Voltagem vs Pressão:**
```
Voltagem (V)  |  Pressão (kPa)  |  Pressão (bar)
--------------|-----------------|---------------
    0.5       |        0        |     0.00
    1.0       |       25        |     0.25
    1.5       |       75        |     0.75
    2.0       |      125        |     1.25
    2.5       |      175        |     1.75
    3.0       |      225        |     2.25
    3.5       |      250        |     2.50
    4.0       |      275        |     2.75
    4.5       |      300        |     3.00
```

**Instalação:**
- Montar no coletor de admissão (usar tomada de vácuo existente)
- Usar mangueira de vácuo (não pode ter vazamento)
- Posição: após a borboleta, antes dos runners

**⚠️ SCG-ECU:**
- Pin: PA3 (ADC1_IN3)
- Calibração no TunerStudio: "GM 3-Bar MAP"

---

## 💨 SENSORES DE OXIGÊNIO (LAMBDA)

### Wideband O2 Sensor (LSU 4.2 - ORIGINAL BMW)

**Quantidade Original:** 2x (bank 1 + bank 2)
**Part Number:** Bosch 0 258 007 057 (BMW 11781427884)
**Tipo:** Bosch LSU 4.2 wideband

**Especificações:**
```
Range Lambda:         0.65 - ∞ (lean)
Range AFR:            9.5 - ∞
Temperatura Operação: 650-850°C
Heater Power:         ~8W @ 12V
Heater Resistance:    ~3Ω
Pump Cell Voltage:    0-1V (controlled by ECU)
Nernst Voltage:       ~450mV @ λ=1
```

**Pinout LSU 4.2:**
```
Pin 1: IP (Pump Current +)
Pin 2: VS (Nernst Voltage + / Virtual Ground)
Pin 3: RTRIM (calibration resistor)
Pin 4: VS (Nernst Voltage - / Ground)
Pin 5: HEATER -
Pin 6: HEATER +
```

**⚠️ SCG-ECU:**
- Possui wideband controller integrado **LSU 4.9**
- **Opção 1:** Reutilizar sensor BMW LSU 4.2 (compatível com pequeno ajuste)
- **Opção 2:** Substituir por Bosch LSU 4.9 (recomendado)
- Usar apenas **1 sensor** (SCG-ECU tem 1 controller)
- Montar em bank 1 (cilindros 1-2-3)

**Sensor Recomendado (LSU 4.9):**
```
Part Number:          Bosch 0 258 017 025
Range Lambda:         0.65 - ∞
Melhor precisão:      ±0.01 λ
Heater:               ~10W @ 12V
```

---

### Narrowband O2 Sensor (Downstream - ORIGINAL BMW)

**Quantidade:** 2x (após catalisadores)
**Função:** Monitoramento de catalisadores (emissões)
**Tipo:** Switching sensor (narrowband)

**Especificações:**
```
Range de Voltagem:    0.1V (lean) → 0.9V (rich)
Switching Point:      ~450mV @ λ=1
Temperatura Operação: 300-850°C
```

**⚠️ SCG-ECU:**
- **Opcional** - não necessário para operação do motor
- Pode ser removido (apenas para diagnóstico de catalisadores)

---

## 🔊 SENSORES DE DETONAÇÃO

### Knock Sensors (2x)

**Quantidade:** 2 sensores
**Localização:** Bloco do motor, entre cilindros
**Part Number:** BMW 13627537453

**Posições:**
```
Knock Sensor 1:  Entre cilindros 2-3
Knock Sensor 2:  Entre cilindros 4-5
```

**Especificações:**
```
Tipo:                 Piezoelectric (ressonante)
Frequência Central:   ~8 kHz (faixa 5-15 kHz)
Sensibilidade:        ~50 mV/g
Resistência Interna:  ~4.5 MΩ @ 1 kHz
Capacitância:         ~1 nF
```

**Funcionamento:**
- Detecta vibrações de detonação no bloco
- Gera pulso de voltagem AC quando detonação ocorre
- ECU analisa frequência e amplitude do sinal

**⚠️ SCG-ECU:**
- Suporta até 2 knock sensors
- Pins: PC8, PC9
- Pode reutilizar sensores BMW originais
- Calibração de threshold no TunerStudio

---

## ⚙️ ATUADORES - INJEÇÃO

### Fuel Injectors (6x)

**Quantidade:** 6 injetores individuais
**Part Number:** BMW 13537546245 (OEM)
**Tipo:** Saturated (high impedance)

**Especificações OEM:**
```
Flow Rate:            236 cc/min @ 3 bar (dynamic)
                      282 cc/min @ 3 bar (static)
Resistência:          11.7-14.9 Ω (nominal 12-13Ω)
Tipo de Controle:     PWM peak-and-hold (mas alta impedância)
Pressão Nominal:      3.5 bar (51 PSI)
Dead Time:            ~0.8 ms @ 14V
                      ~1.2 ms @ 12V
```

**Curva de Dead Time:**
```
Voltagem (V)  |  Dead Time (ms)
--------------|----------------
     10       |      1.8
     11       |      1.4
     12       |      1.2
     13       |      1.0
     14       |      0.8
```

**Firing Order:** 1-5-3-6-2-4

**Upgrade para Turbo:**
```
Flow Rate:            440 cc/min @ 3 bar
                      600 cc/min @ 4 bar
Exemplos:             Bosch EV14, Injector Dynamics ID1000
Resistência:          ~11-14 Ω (saturated)
```

**⚠️ SCG-ECU:**
- Totalmente compatível (saturated type)
- 6x saídas low-side drivers (7A)
- Configurar dead time no TunerStudio
- Pins: PE15, PE14, PE13, PE12, PE11, PE10

---

### Fuel Pump

**Tipo:** In-tank electric pump
**Pressão Nominal:** 3.5 bar (51 PSI)
**Flow Rate:** ~120 L/h @ 3.5 bar
**Part Number:** BMW 16146765121 (pump module)

**Controle:**
- Relé controlado pela ECU
- Pin MS43: X60001-3
- Voltagem: 12V switched

**⚠️ SCG-ECU:**
- Usar saída auxiliar (relé)
- Configurar como "Fuel Pump Output" no TunerStudio

---

## 🔥 ATUADORES - IGNIÇÃO

### Ignition Coils (6x)

**Quantidade:** 6 bobinas individuais (coil-on-plug)
**Part Number:** Bosch 0 221 504 470 (OEM)
**Tipo:** Pencil coil (stick coil)

**Especificações:**
```
Resistência Primária:    0.8 Ω ±10% (between pins 1 and 15)
Resistência Secundária:  1.0 kΩ ±20% (old style)
                         1.8 kΩ ±20% (new style)
Indutância Primária:     ~6 mH
Dwell Time:              3.0-3.5 ms @ 14V
                         4.0-4.5 ms @ 12V
Max Spark Energy:        ~50 mJ
Voltagem Secundária:     ~30-40 kV
```

**Pinout (2-pin connector):**
```
Pin 1:  12V+ (from relay)
Pin 15: Ground (switched by ECU)
```

**Controle:**
- Ground-switched (low-side driver)
- Corrente primária: ~15A peak

**Firing Order:** 1-5-3-6-2-4

**⚠️ SCG-ECU:**
- Totalmente compatível
- 6x saídas high-side drivers (5V/12V logic)
- Dwell configurável no TunerStudio
- Pins: PD12, PD13, PB15, PB14, PD8, PD9

---

### Spark Plugs

**Part Number:** NGK BCPR6ES-11 (OEM)
**Alternativa:** Bosch FR7LDC+

**Especificações:**
```
Gap:                  0.9-1.0 mm (0.035-0.040 in)
Thread:               M14 x 1.25
Reach:                19 mm
Heat Range:           6 (medium)
Type:                 Copper core
```

**⚠️ SCG-ECU:**
- Sem mudanças necessárias
- Manter gap OEM

---

## 🌀 ATUADORES - VANOS (VVT)

### VANOS Solenoids (2x)

**Sistema:** Double VANOS (intake + exhaust)
**Part Number:** BMW 11-36-1-440-142 (pair)
**Tipo:** Oil pressure control valve (PWM)

**Especificações:**
```
Resistência:          ~10-15 Ω
Voltagem:             12V PWM
Duty Cycle Range:     0-100%
Frequency:            100-250 Hz (típico)
Corrente Máxima:      ~1.2A
```

**Controle:**
```
0% Duty:    Posição retraída (sem avanço)
50% Duty:   Posição intermediária
100% Duty:  Posição totalmente avançada
```

**Adjustment Range:**
```
Intake:     ~40° camshaft degrees (80° crank degrees)
Exhaust:    ~20° camshaft degrees (40° crank degrees)
```

**⚠️ SCG-ECU:**
- Totalmente compatível (VVT1 + VVT2)
- Controle closed-loop com feedback dos position sensors
- Pins PWM: PC6 (intake), PC7 (exhaust)
- Pins Feedback: PA5 (intake), PA6 (exhaust)

---

## 🎛️ OUTROS ATUADORES

### DISA Valve

**Sistema:** Dual Intake System Adjustment
**Função:** Altera comprimento dos runners de admissão
**Part Number:** BMW 11617544805
**Tipo:** Vacuum-operated solenoid

**Especificações:**
```
Resistência:          ~50 Ω
Voltagem:             12V
Tipo de Controle:     On/Off (não PWM)
Transição:            ~3500 RPM
```

**Estados:**
```
LOW RPM (< 3500):   Long runners (melhor torque baixa)
HIGH RPM (> 3500):  Short runners (melhor potência alta)
```

**⚠️ SCG-ECU:**
- Usar saída auxiliar on/off
- Configurar como "DISA Output" baseado em RPM threshold

---

### EVAP Purge Valve

**Função:** Controle de emissões (purgar vapores do canister)
**Part Number:** BMW 11747512100
**Tipo:** PWM solenoid

**Especificações:**
```
Resistência:          ~25-30 Ω
Voltagem:             12V PWM
Frequency:            10-20 Hz
Duty Cycle Range:     0-100%
```

**⚠️ SCG-ECU:**
- Suportado nativamente
- Configurar como "Canister Purge" no TunerStudio
- Usar saída auxiliar PWM

---

## 📊 RESUMO DE COMPATIBILIDADE

| Sensor/Atuador | Tipo | BMW Part Number | SCG-ECU Pin | Status | Observação |
|----------------|------|-----------------|-------------|--------|------------|
| **CLT** | NTC | 13621433077 | PA0 | ✅ 100% | Curva configurável |
| **IAT** | NTC | 13621747155 | PA1 | ✅ 100% | Usar standalone |
| **CKP** | Hall 60-2 | 12141247978 | PA15 | ✅ 100% | MISSING_TOOTH |
| **CMP Intake** | Hall | 12141438082 | PB3 | ✅ 100% | Trigger secundário |
| **TPS** | Potentiometer | *(aftermarket)* | PA2 | ⚠️ Trocar | Usar TPS mecânico |
| **MAP** | Analog | *(GM 3-bar)* | PA3 | ⚠️ Adicionar | Novo sensor |
| **O2 Wideband** | LSU 4.9 | 0 258 017 025 | PA4 | ✅ 95% | Trocar para LSU 4.9 |
| **Knock Sensors** | Piezo | 13627537453 | PC8, PC9 | ✅ 100% | Reutilizar BMW |
| **Injetores** | Saturated | 13537546245 | PE15-10 | ✅ 100% | 6x outputs |
| **Bobinas** | COP | 0 221 504 470 | PD12-9 | ✅ 100% | 6x outputs |
| **VANOS** | PWM Solenoid | 11-36-1-440-142 | PC6, PC7 | ✅ 100% | Dual VVT |

**Compatibilidade Geral:** ✅ **90%** (trocar TPS + adicionar MAP)

---

**Última atualização:** 2025-11-07
**Fonte:** BMW TIS, Bosch Datasheets, MS4X Wiki
**Versão:** 1.0
