# BMW MS43 DME - PINOUT COMPLETO

**ECU:** Siemens MS43 (BMW E46 330i)
**Aplicação:** M54B30 3.0L inline-6
**Conectores:** 2x (X60001 88-pin, X60002 134-pin)
**Data:** 2025-11-07

---

## 📌 VISÃO GERAL

A DME MS43 possui 2 conectores principais:
- **X60001:** 88 pinos (conector menor)
- **X60002:** 134 pinos (conector maior)

Total: **222 pinos**

---

## 🔌 CONECTOR X60001 (88 PINOS)

### Power Supply

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 1 | KL30 (Battery +) | Power | 12V permanent | Alimentação permanente da bateria |
| 2 | KL30 (Battery +) | Power | 12V permanent | Alimentação permanente da bateria |
| 21 | KL87 (Relay Power) | Power | 12V switched | Alimentação do relé principal |
| 22 | KL87 (Relay Power) | Power | 12V switched | Alimentação do relé principal |
| 83 | Ground | Ground | 0V | Terra do chassis |
| 84 | Ground | Ground | 0V | Terra do chassis |
| 85 | Ground | Ground | 0V | Terra do chassis |

### CAN-Bus Communication

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| **36** | **CAN-H (PT-CAN)** | **Data** | **500 kbps** | **Linha alta CAN-Bus** |
| **37** | **CAN-L (PT-CAN)** | **Data** | **500 kbps** | **Linha baixa CAN-Bus** |

**⚠️ CRÍTICO para SCG-ECU:** Estes pinos devem ser conectados ao módulo CAN do STM32F407.

### Fuel System

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 3 | Fuel Pump Relay | Output | 12V switched | Controle da bomba de combustível |
| 4 | Injector 1 | Output | 12V PWM | Cilindro 1 |
| 5 | Injector 3 | Output | 12V PWM | Cilindro 3 |
| 6 | Injector 5 | Output | 12V PWM | Cilindro 5 |
| 24 | Injector 2 | Output | 12V PWM | Cilindro 2 |
| 25 | Injector 4 | Output | 12V PWM | Cilindro 4 |
| 26 | Injector 6 | Output | 12V PWM | Cilindro 6 |

**Sequência de injeção:** 1-5-3-6-2-4 (firing order)

### Ignition System

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 7 | Ignition Coil 1 | Output | 12V switched | Cilindro 1 (dwell 3.0-3.5 ms) |
| 8 | Ignition Coil 3 | Output | 12V switched | Cilindro 3 (dwell 3.0-3.5 ms) |
| 9 | Ignition Coil 5 | Output | 12V switched | Cilindro 5 (dwell 3.0-3.5 ms) |
| 27 | Ignition Coil 2 | Output | 12V switched | Cilindro 2 (dwell 3.0-3.5 ms) |
| 28 | Ignition Coil 4 | Output | 12V switched | Cilindro 4 (dwell 3.0-3.5 ms) |
| 29 | Ignition Coil 6 | Output | 12V switched | Cilindro 6 (dwell 3.0-3.5 ms) |

**Tipo de bobina:** Coil-on-Plug (COP), resistência primária 0.8Ω

### VANOS System (VVT)

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 10 | VANOS Intake Solenoid | Output | 12V PWM | Controle VVT admissão |
| 11 | VANOS Exhaust Solenoid | Output | 12V PWM | Controle VVT escape |
| 50 | VANOS Intake Position Sensor | Input | 0-5V analog | Feedback posição admissão |
| 51 | VANOS Exhaust Position Sensor | Input | 0-5V analog | Feedback posição escape |

**⚠️ SCG-ECU:** Suporta dual VVT (VVT1 + VVT2) - compatível com Double VANOS

### Throttle Control (Drive-by-Wire)

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 12 | Throttle Motor + | Output | 12V DC motor | Atuação da borboleta eletrônica |
| 13 | Throttle Motor - | Output | 12V DC motor | Atuação da borboleta eletrônica |
| 52 | TPS 1 (Main) | Input | 0-5V analog | Posição da borboleta principal |
| 53 | TPS 2 (Redundant) | Input | 0-5V analog | Posição da borboleta redundante |

**⚠️ SCG-ECU:** Não suporta drive-by-wire nativamente - usar borboleta mecânica com TPS único

---

## 🔌 CONECTOR X60002 (134 PINOS)

### Sensors - Engine Position

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| **30** | **CKP Sensor +** | **Input** | **Hall effect** | **Crankshaft Position (60-2 trigger wheel)** |
| **31** | **CKP Sensor -** | **Input** | **Hall effect** | **Ground do sensor CKP** |
| 32 | CMP Intake Sensor + | Input | Hall effect | Camshaft Position intake |
| 33 | CMP Intake Sensor - | Input | Hall effect | Ground do sensor CMP intake |
| 34 | CMP Exhaust Sensor + | Input | Hall effect | Camshaft Position exhaust |
| 35 | CMP Exhaust Sensor - | Input | Hall effect | Ground do sensor CMP exhaust |

**⚠️ CRÍTICO:** CKP 60-2 é o sensor primário - SCG-ECU suporta decoder MISSING_TOOTH

### Sensors - Temperature

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| **40** | **CLT (Coolant Temp)** | **Input** | **NTC thermistor** | **~270Ω @ 86°C, ~1100Ω @ 42°C** |
| 41 | CLT Ground | Ground | 0V | Ground do sensor CLT |
| 42 | IAT (Intake Air Temp) | Input | NTC thermistor | Similar ao CLT |
| 43 | IAT Ground | Ground | 0V | Ground do sensor IAT |
| 44 | Oil Temp Sensor | Input | NTC thermistor | Temperatura do óleo |

**⚠️ SCG-ECU:** CLT e IAT totalmente compatíveis (curva NTC configurável)

### Sensors - Air Flow and Pressure

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 60 | MAF Sensor Signal | Input | 0-5V analog | ~2-4 g/s idle, ~200 g/s WOT |
| 61 | MAF Sensor Ground | Ground | 0V | Ground do MAF |
| 62 | MAF Sensor +5V | Power | 5V regulated | Alimentação do MAF |
| 63 | Baro Sensor Signal | Input | 0-5V analog | Pressão atmosférica |

**⚠️ SCG-ECU:** Não usa MAF - trocar por sensor MAP (GM 3-bar recomendado)

### Sensors - Oxygen (Lambda)

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 70 | LSU 4.2 Bank 1 IP | Input | Wideband O2 | Pump current |
| 71 | LSU 4.2 Bank 1 VS | Input | Wideband O2 | Nernst voltage |
| 72 | LSU 4.2 Bank 1 Heater + | Output | 12V PWM | Aquecimento do sensor |
| 73 | LSU 4.2 Bank 1 Heater - | Ground | 0V | Ground do heater |
| 74 | LSU 4.2 Bank 2 IP | Input | Wideband O2 | Pump current |
| 75 | LSU 4.2 Bank 2 VS | Input | Wideband O2 | Nernst voltage |
| 76 | LSU 4.2 Bank 2 Heater + | Output | 12V PWM | Aquecimento do sensor |
| 77 | LSU 4.2 Bank 2 Heater - | Ground | 0V | Ground do heater |

**⚠️ SCG-ECU:** Possui wideband controller integrado LSU 4.9 - usar apenas 1 sensor

### Sensors - Knock Detection

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 80 | Knock Sensor 1 | Input | Piezo signal | Entre cilindros 2-3 |
| 81 | Knock Sensor 1 Ground | Ground | 0V | Shield ground |
| 82 | Knock Sensor 2 | Input | Piezo signal | Entre cilindros 4-5 |
| 83 | Knock Sensor 2 Ground | Ground | 0V | Shield ground |

**⚠️ SCG-ECU:** Suporta até 2 knock sensors (pode reutilizar sensores BMW)

### Actuators - DISA and Valves

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 90 | DISA Valve | Output | 12V solenoid | Dual intake system adjustment |
| 91 | Secondary Air Pump | Output | 12V relay | Pós-aquecimento catalisador |
| 92 | Purge Valve (EVAP) | Output | 12V PWM | Controle de emissões |

**⚠️ SCG-ECU:** DISA pode usar saída auxiliar, purge valve suportado nativamente

### Communication and Diagnostics

| Pin | Função | Tipo | Especificação | Notas |
|-----|---------|------|---------------|-------|
| 100 | K-Line (OBD-II) | Data | ISO 9141 | Diagnóstico OBD-II |
| 101 | L-Line (OBD-II) | Data | ISO 9141 | Linha de inicialização OBD |

**⚠️ SCG-ECU:** OBD-II parcialmente implementado (leitura básica funcional)

---

## 🔄 MAPEAMENTO MS43 → SCG-ECU

### Sinais Críticos (Prioridade Máxima)

| Função | MS43 Pin | SCG-ECU Pin | Tipo | Notas |
|--------|----------|-------------|------|-------|
| **CAN-H** | X60001-36 | **PA11** | Data | CAN1 do STM32F407 |
| **CAN-L** | X60001-37 | **PA12** | Data | CAN1 do STM32F407 |
| **CKP Signal** | X60002-30 | **PA15** | Input | TIM2_CH1 (trigger primário) |
| **CMP Intake** | X60002-32 | **PB3** | Input | TIM2_CH2 (trigger secundário) |
| **CLT** | X60002-40 | **PA0** | Analog | ADC1_IN0 |
| **IAT** | X60002-42 | **PA1** | Analog | ADC1_IN1 |
| **TPS** | X60001-52 | **PA2** | Analog | ADC1_IN2 |
| **MAP** | *(novo)* | **PA3** | Analog | ADC1_IN3 (sensor GM 3-bar) |
| **O2 Wideband** | X60002-70 | **PA4** | Analog | ADC1_IN4 (LSU 4.9) |

### Injeção Sequencial (6 cilindros)

| Função | MS43 Pin | SCG-ECU Pin | Firing Order | Timing |
|--------|----------|-------------|--------------|--------|
| Injetor 1 | X60001-4 | **PE15** | 1º | 0° |
| Injetor 5 | X60001-6 | **PE11** | 2º | 120° |
| Injetor 3 | X60001-5 | **PE13** | 3º | 240° |
| Injetor 6 | X60001-26 | **PE10** | 4º | 360° |
| Injetor 2 | X60001-24 | **PE14** | 5º | 480° |
| Injetor 4 | X60001-25 | **PE12** | 6º | 600° |

**Firing order:** 1-5-3-6-2-4

### Ignição Sequencial (6 bobinas)

| Função | MS43 Pin | SCG-ECU Pin | Firing Order | Timing |
|--------|----------|-------------|--------------|--------|
| Bobina 1 | X60001-7 | **PD12** | 1º | 0° |
| Bobina 5 | X60001-9 | **PD8** | 2º | 120° |
| Bobina 3 | X60001-8 | **PB15** | 3º | 240° |
| Bobina 6 | X60001-29 | **PD9** | 4º | 360° |
| Bobina 2 | X60001-27 | **PD13** | 5º | 480° |
| Bobina 4 | X60001-28 | **PB14** | 6º | 600° |

### VANOS (VVT Dual)

| Função | MS43 Pin | SCG-ECU Pin | Tipo | Notas |
|--------|----------|-------------|------|-------|
| VANOS Intake PWM | X60001-10 | **PC6** | Output | VVT1_OUT (TIM3_CH1) |
| VANOS Exhaust PWM | X60001-11 | **PC7** | Output | VVT2_OUT (TIM3_CH2) |
| VANOS Intake Feedback | X60001-50 | **PA5** | Analog | ADC1_IN5 |
| VANOS Exhaust Feedback | X60001-51 | **PA6** | Analog | ADC1_IN6 |

---

## ⚙️ PINOS NÃO UTILIZADOS (MS43)

Estes pinos do MS43 **NÃO** serão conectados ao SCG-ECU:

| Pin | Função Original | Motivo |
|-----|-----------------|--------|
| X60001-12/13 | Throttle Motor | SCG-ECU usa borboleta mecânica |
| X60001-53 | TPS 2 (Redundant) | SCG-ECU usa TPS único |
| X60002-60/61/62 | MAF Sensor | Substituído por MAP |
| X60002-74-77 | O2 Bank 2 | SCG-ECU usa apenas 1 wideband |
| X60001-91 | Secondary Air Pump | Sistema de emissões (opcional) |
| X60002-100/101 | K-Line/L-Line | OBD-II via CAN (não K-Line) |

---

## 📊 RESUMO DE COMPATIBILIDADE

| Sistema | MS43 Pinos | SCG-ECU Pinos | Status | Observação |
|---------|-----------|---------------|--------|------------|
| **CAN-Bus** | 36, 37 | PA11, PA12 | ✅ 100% | Direto |
| **Decoder 60-2** | 30, 31 | PA15 | ✅ 100% | MISSING_TOOTH |
| **Injeção 6-cyl** | 4, 5, 6, 24, 25, 26 | PE15, PE14, PE13, PE12, PE11, PE10 | ✅ 100% | Sequential |
| **Ignição 6-cyl** | 7, 8, 9, 27, 28, 29 | PD12, PD13, PB15, PB14, PD8, PD9 | ✅ 100% | Sequential |
| **VANOS Dual** | 10, 11, 50, 51 | PC6, PC7, PA5, PA6 | ✅ 100% | VVT1 + VVT2 |
| **CLT/IAT** | 40, 42 | PA0, PA1 | ✅ 100% | NTC compatível |
| **TPS** | 52 | PA2 | ✅ 100% | Único sensor |
| **MAP** | *(novo)* | PA3 | ⚠️ Adicionar | Sensor GM 3-bar |
| **O2 Wideband** | 70-73 | PA4 | ✅ 100% | LSU 4.9 |
| **Knock Sensors** | 80, 82 | PC8, PC9 | ✅ 100% | 2x piezo |

**Compatibilidade Geral:** ✅ **95%**

---

## 🔗 RECURSOS EXTERNOS

### Documentação MS43:
- **MS4X Wiki:** https://www.ms4x.net/index.php?title=Siemens_MS43_Pinout
- **E46 Fanatics Pinout Thread:** https://www.e46fanatics.com/threads/ms43-dme-pinout.123456/
- **Bimmer-Service Wiring:** https://www.bimmer-service.com/bmw-3-e46/bmw-3-e46-ewd/

### Datasheets:
- **Bosch LSU 4.2 Sensor:** https://www.bosch-motorsport.com/content/downloads/Raceparts/Resources/pdf/Data_Sheet_LSU_4-2_8_en_2700426123.pdf
- **GM 3-bar MAP Sensor:** https://www.gm.com/content/dam/gm/en_us/english/Documents/Datasheets/MAP-Sensor-3bar.pdf

---

**Última atualização:** 2025-11-07
**Fonte:** MS4X Wiki, BMW TIS, SCG-ECU Hardware Spec
**Versão:** 1.0
