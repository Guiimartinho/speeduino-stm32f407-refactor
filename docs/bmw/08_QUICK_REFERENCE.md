# BMW 330i E46 - QUICK REFERENCE

**Motor:** M54B30 3.0L inline-6
**ECU:** SCG-ECU 2.0 STM32F407VGT6
**Veículo:** BMW E46 330i (2000-2006)
**Data:** 2025-11-07

---

## 🎯 ESPECIFICAÇÕES RÁPIDAS

### Motor

```
Displacement:            2979 cc (2.98 L)
Cilindros:               6 em linha
Bore x Stroke:           84.0 x 89.6 mm
Compression Ratio:       10.2:1
Potência:                231 HP @ 5900 RPM
Torque:                  300 Nm @ 3500 RPM
Redline:                 6500 RPM
Fuel Cut:                6750 RPM
Firing Order:            1-5-3-6-2-4
```

### Trigger Setup

```
Decoder:                 MISSING_TOOTH (60-2)
Trigger Angle:           90° BTDC cyl #1
CKP Sensor:              Hall effect (BMW 12141247978)
CMP Sensor:              Hall effect (BMW 12141438082)
Missing Teeth:           2 (a 90° BTDC)
```

---

## 📊 VALORES DE CALIBRAÇÃO PRONTOS

### Sensores

**CLT (Coolant Temperature):**
```
Tipo:                    NTC thermistor
Pull-up:                 2200 Ω @ 5V
Resistências:
  @ 20°C:                2500 Ω
  @ 80°C:                300 Ω
  @ 100°C:               180 Ω
```

**IAT (Intake Air Temperature):**
```
Tipo:                    NTC thermistor (idêntico CLT)
Pull-up:                 2200 Ω @ 5V
```

**TPS (Throttle Position):**
```
Tipo:                    Potentiometer 0-5V
Closed:                  0.5V → 0%
Open:                    4.5V → 100%
```

**MAP (Manifold Pressure):**
```
Sensor:                  GM 3-Bar
0 kPa:                   0.5V
100 kPa:                 2.0V (atmospheric)
200 kPa:                 3.5V
300 kPa:                 4.5V
```

**O2 Wideband:**
```
Sensor:                  LSU 4.9 (Bosch 0 258 017 025)
Lambda Range:            0.65 - 1.30
Free Air Cal:            Fazer antes de cada partida
Target Lambda:
  Idle/Cruise:           1.00 (14.7 AFR)
  WOT:                   0.85-0.90 (12.5-13.2 AFR)
```

---

### Injetores

**OEM BMW:**
```
Part Number:             13537546245
Flow Rate:               236 cc/min @ 3 bar (dynamic)
                         282 cc/min @ 3 bar (static)
Resistência:             12-13 Ω (saturated)
Pressão:                 3.5 bar (51 PSI)

Dead Time:
  @ 14V:                 0.8 ms
  @ 13V:                 1.0 ms
  @ 12V:                 1.2 ms
  @ 11V:                 1.4 ms
```

**Upgrade Turbo:**
```
Flow Rate:               440 cc/min @ 3 bar
                         600 cc/min @ 4 bar
Exemplos:                Bosch EV14, ID1000
Dead Time:               ~1.0 ms @ 14V (ajustar conforme spec)
```

---

### Ignição

**Bobinas:**
```
Part Number:             Bosch 0 221 504 470
Tipo:                    Pencil coil (COP)
Resistência Primária:    0.8 Ω
Resistência Secundária:  1.0-1.8 kΩ

Dwell Time:
  @ 14V:                 3.2 ms
  @ 13V:                 3.5 ms
  @ 12V:                 4.0 ms
  @ 11V:                 4.5 ms

Max Dwell:               6.0 ms (safety)
```

**Velas:**
```
Part Number:             NGK BCPR6ES-11
Gap:                     0.9-1.0 mm (0.035-0.040 in)
Torque:                  30 Nm (22 ft-lb)
```

---

### VANOS (VVT)

**Solenoids:**
```
Part Number:             BMW 11-36-1-440-142
Resistência:             10-15 Ω
PWM Frequency:           200 Hz
Duty Range:              0-100%
```

**Position Sensors:**
```
Range:                   0.5V → 4.5V
Intake Max Advance:      40° (camshaft degrees)
Exhaust Max Advance:     20° (camshaft degrees)
```

**PID Values:**
```
P:                       1.5
I:                       0.3
D:                       0.05
```

---

## 📋 TABELAS BASE (COPY-PASTE READY)

### VE Table (Volumetric Efficiency)

```
RPM/MAP →  30kPa  50kPa  70kPa  90kPa  100kPa
1000       40     50     60     70     75
1500       42     52     62     72     77
2000       45     55     65     75     80
2500       48     58     68     78     83
3000       50     60     70     80     85
3500       52     62     72     82     87
4000       54     64     74     84     89
4500       55     65     75     85     90
5000       56     66     76     86     91
5500       54     64     74     84     89
6000       52     62     72     82     87
6500       48     58     68     78     83
```

### Spark Advance Table (95 RON)

```
RPM/MAP →  30kPa  50kPa  70kPa  90kPa  100kPa
1000       15     12     10     8      6
1500       18     15     12     10     8
2000       22     18     15     12     10
2500       26     22     18     15     12
3000       30     26     22     18     15
3500       32     28     24     20     17
4000       34     30     26     22     19
4500       35     31     27     23     20
5000       36     32     28     24     21
5500       34     30     26     22     19
6000       32     28     24     20     17
6500       30     26     22     18     15
```

### AFR Target Table

```
RPM/MAP →  30kPa  50kPa  70kPa  90kPa  100kPa
1000       15.0   14.7   14.5   14.0   13.5
1500       15.0   14.7   14.5   14.0   13.2
2000       15.2   14.7   14.3   13.8   13.0
2500       15.2   14.7   14.3   13.5   12.8
3000       15.2   14.7   14.0   13.2   12.5
3500       15.0   14.7   13.8   13.0   12.3
4000       15.0   14.7   13.5   12.8   12.0
4500       14.7   14.5   13.2   12.5   12.0
5000       14.7   14.3   13.0   12.3   11.8
5500       14.7   14.0   12.8   12.0   11.8
6000       14.7   13.8   12.5   12.0   11.8
```

### VVT1 Intake Timing Table

```
RPM/MAP →  30kPa  50kPa  70kPa  90kPa  100kPa
1000       0      5      10     15     18
2000       5      10     15     20     23
3000       10     15     20     25     28
4000       15     20     25     30     33
5000       20     25     30     35     38
6000       15     20     25     30     33
```

### VVT2 Exhaust Timing Table

```
RPM/MAP →  30kPa  50kPa  70kPa  90kPa  100kPa
1000       0      0      2      5      8
2000       0      2      5      8      10
3000       2      5      8      10     12
4000       5      8      10     12     15
5000       8      10     12     15     18
6000       5      8      10     12     15
```

---

## 🔌 PINOUT SCG-ECU

### Inputs (Sensores)

```
PA0  →  CLT (Coolant Temperature)
PA1  →  IAT (Intake Air Temperature)
PA2  →  TPS (Throttle Position)
PA3  →  MAP (Manifold Absolute Pressure)
PA4  →  O2 (Wideband LSU 4.9)
PA5  →  VVT1 Position (Intake)
PA6  →  VVT2 Position (Exhaust)

PA15 →  CKP (Crankshaft Position - trigger primário)
PB3  →  CMP (Camshaft Position - trigger secundário)

PC8  →  Knock Sensor 1 (between cyl 2-3)
PC9  →  Knock Sensor 2 (between cyl 4-5)
```

### Outputs (Atuadores)

**Injetores (Firing Order: 1-5-3-6-2-4):**
```
PE15 →  Injector 1 (0°)
PE11 →  Injector 5 (120°)
PE13 →  Injector 3 (240°)
PE10 →  Injector 6 (360°)
PE14 →  Injector 2 (480°)
PE12 →  Injector 4 (600°)
```

**Bobinas (Firing Order: 1-5-3-6-2-4):**
```
PD12 →  Coil 1 (0°)
PD8  →  Coil 5 (120°)
PB15 →  Coil 3 (240°)
PD9  →  Coil 6 (360°)
PD13 →  Coil 2 (480°)
PB14 →  Coil 4 (600°)
```

**VANOS (VVT):**
```
PC6  →  VVT1 PWM (Intake)
PC7  →  VVT2 PWM (Exhaust)
```

### Communication

```
PA11 →  CAN-H (CAN1_RX)
PA12 →  CAN-L (CAN1_TX)
```

---

## 🚨 TROUBLESHOOTING RÁPIDO

### Motor não pega

```
Sintoma                  | Verificar
------------------------|---------------------------
RPM = 0 no TS           | CKP sensor wiring (PA15)
RPM OK, mas não pega    | Injetores (teste clique)
RPM OK, injeção OK      | Bobinas (spark test)
Pega e morre            | Trigger angle (ajustar)
Backfire                | Ignition timing (retardar)
```

### Idle Problems

```
Sintoma                  | Possível Causa
------------------------|---------------------------
Oscila ±100 RPM         | VANOS seals desgastadas
Idle muito baixo        | Leak de vácuo
Idle muito alto         | TPS não calibrado
Rough idle              | VE table incorreta
```

### CAN-Bus Issues

```
Sintoma                  | Solução
------------------------|---------------------------
Painel não mostra RPM   | Ativar BMW DME protocol
Dados errados           | Verificar baud 500kbps
Nenhum dado CAN         | Adicionar terminação 120Ω
```

### VANOS Problems

```
Sintoma                  | Solução
------------------------|---------------------------
Position não muda       | Verificar oil pressure
Response lento          | Limpar/trocar solenoids
Oscilação em idle       | Rebuild VANOS seals
```

---

## ⚡ COMANDOS RÁPIDOS TUNERSTUDIO

### Calibração Inicial

```
1. Tools → Calibrate TPS
   - Fechar borboleta → Set Closed (0.5V)
   - Abrir totalmente → Set Open (4.5V)

2. Tools → Test Outputs
   - Testar cada injetor (deve clicar)
   - Testar cada bobina (spark test)

3. Tools → Trigger Wizard
   - Verificar sync (deve mostrar "Synced")
   - Ajustar trigger angle se necessário

4. Tools → AFR Calibration
   - Free air cal (antes de cada partida)
   - Verificar lambda ~1.00 em idle
```

### Data Logging Essencial

**Canais Obrigatórios:**
```
- Time (timestamp)
- RPM
- MAP (kPa)
- TPS (%)
- AFR (Air-Fuel Ratio)
- Lambda
- Spark Advance (deg)
- VE (%)
- CLT (°C)
- IAT (°C)
- VVT1 Target (deg)
- VVT1 Actual (deg)
- VVT2 Target (deg)
- VVT2 Actual (deg)
- Knock Count
```

---

## 🔧 VALORES DE PROTEÇÃO

### Engine Limits

```
Hard Rev Limit:          6750 RPM (fuel + ign cut)
Soft Rev Limit:          6500 RPM (ign retard -10°)
CLT Warning:             105°C (fan high)
CLT Critical:            115°C (power cut 50%)
CLT Emergency:           120°C (engine shutdown)
IAT Warning:             55°C (retard -3°)
IAT Critical:            65°C (retard -6°)
```

### Knock Control

```
Threshold:               30% (initial)
Retard per Event:        3°
Max Retard:              10°
Recovery Rate:           1° per second
AFR Enrichment:          5% on knock
```

---

## 📦 PEÇAS DE REPOSIÇÃO (PART NUMBERS)

### Sensores

```
CLT:                     BMW 13621433077
IAT:                     BMW 13621747155
CKP:                     BMW 12141247978
CMP:                     BMW 12141438082
Knock Sensor:            BMW 13627537453
O2 Wideband:             Bosch 0 258 017 025 (LSU 4.9)
MAP:                     GM 12223861 (ACDelco 213-796)
```

### Atuadores

```
Injetores:               BMW 13537546245 (236cc OEM)
Bobinas:                 Bosch 0 221 504 470
VANOS Solenoid (pair):   BMW 11-36-1-440-142
VANOS Rebuild Kit:       BMW 11-36-7-833-663
Velas:                   NGK BCPR6ES-11
```

### Consumíveis

```
Óleo Motor:              5W-30 / 5W-40 BMW LL-01
Quantidade:              6.5 L (com filtro)
Filtro de Óleo:          BMW 11427512300 (Mahle/Mann)
Velas (6x):              NGK BCPR6ES-11
Bobinas (6x):            Bosch 0 221 504 470
```

---

## 🎯 CHECKLIST PRIMEIRA PARTIDA

### Pré-Start (Motor Desligado)

```
☐ Nível de óleo OK (6.5L)
☐ Nível de combustível > 1/4 tank
☐ Bateria carregada (12.5V+)
☐ TunerStudio conectado e logging
☐ Desconectar bobinas (cranking test)
☐ Desabilitar bomba (cranking test)
```

### Cranking Test (Sem Combustão)

```
☐ Cranking por 5 segundos
☐ RPM mostra 200-300 RPM
☐ Sync = "Synced"
☐ CLT lê temperatura ambiente
☐ MAP lê 35-45 kPa (vacuum)
☐ TPS lê 0-2% (closed)
```

### First Start

```
☐ Reconectar bobinas
☐ Habilitar bomba + ignição
☐ Prime pump (3 segundos)
☐ Cranking (máx 10 segundos)
☐ Motor pega e idle
☐ Idle estabiliza em 700-750 RPM
☐ CLT sobe gradualmente
☐ AFR ~14.7 em idle quente
```

### Verificações Pós-Start

```
☐ CAN-Bus funcional (painel mostra RPM)
☐ VANOS responde (variar RPM, verificar position)
☐ Sem códigos de erro (check MIL)
☐ Pressão de óleo OK (2-4 bar @ 3000 RPM)
☐ Temperatura estabiliza (86-95°C)
☐ Idle suave (< ±50 RPM oscilação)
```

---

## 💾 BACKUP E VERSIONING

### Arquivos Críticos

```
Base Tune:               BMW_330i_E46_M54B30_base.msq
Working Tune:            BMW_330i_E46_M54B30_v1.0.msq
Dyno Tune:               BMW_330i_E46_M54B30_dyno_YYYY-MM-DD.msq

Backup Location:
  - USB drive (2 cópias)
  - Cloud storage (Dropbox/Google Drive)
  - Laptop local
```

### Naming Convention

```
BMW_330i_E46_M54B30_[status]_[version]_[date].msq

Exemplos:
  BMW_330i_E46_M54B30_base_v1.0_2025-11-07.msq
  BMW_330i_E46_M54B30_street_v2.3_2025-12-15.msq
  BMW_330i_E46_M54B30_dyno_v3.0_2026-01-20.msq
```

---

## 📞 CONTATOS ÚTEIS

### Comunidades

```
E46 Fanatics:            https://www.e46fanatics.com
Speeduino Forum:         https://speeduino.com/forum
MS4X Wiki:               https://www.ms4x.net
```

### Fornecedores

```
FCP Euro:                https://www.fcpeuro.com (OEM parts)
Turner Motorsport:       https://www.turnermotorsport.com
Beisan Systems:          https://www.beisansystems.com (VANOS)
DIY Auto Tune:           https://www.diyautotune.com (sensors)
```

### Datasheets

```
Bosch LSU 4.9:           https://www.bosch-motorsport.com
GM 3-Bar MAP:            https://www.gm.com/datasheets
BMW TIS:                 https://www.bmwtis.net
```

---

**Última atualização:** 2025-11-07
**Versão:** 1.0
**Autor:** SCG-ECU Team

---

🎉 **DOCUMENTAÇÃO COMPLETA!**

Agora você tem tudo que precisa para implementar a SCG-ECU no BMW 330i E46!

**Próximos passos:**
1. Ler documentos na ordem recomendada (ver 00_INDEX.md)
2. Comprar componentes necessários (ver 07_COMPATIBILIDADE_HARDWARE.md)
3. Fabricar chicote adaptador
4. Configurar TunerStudio (ver 06_CONFIGURACAO_SCG_ECU.md)
5. First start!

**Boa sorte! 🚀**
