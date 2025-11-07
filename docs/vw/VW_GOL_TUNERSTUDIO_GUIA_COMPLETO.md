# GUIA COMPLETO: TUNERSTUDIO + SPEEDUINO STM32F407 - VW GOL AP 1.8

**Versão:** 1.0
**Data:** 07/01/2025
**Hardware:** SCG-ECU 2.0 (STM32F407VGT6)
**Motor:** Volkswagen AP 1.8 MI (Gol Quadrado 1994)
**Software:** TunerStudio MS + Speeduino Firmware

---

## ÍNDICE

1. [Introdução](#1-introdução)
2. [Diferenças STM32F407 vs Arduino](#2-diferenças-stm32f407-vs-arduino)
3. [Preparação do Hardware](#3-preparação-do-hardware)
4. [Instalação do TunerStudio](#4-instalação-do-tunerstudio)
5. [Primeira Conexão](#5-primeira-conexão)
6. [Configuração Engine Constants](#6-configuração-engine-constants)
7. [Trigger Setup (Distribuidor/Roda Fônica)](#7-trigger-setup)
8. [Configuração de Sensores](#8-configuração-de-sensores)
9. [Calibração de Sensores](#9-calibração-de-sensores)
10. [Tabelas de Mapeamento](#10-tabelas-de-mapeamento)
11. [Proteções e Limites](#11-proteções-e-limites)
12. [Primeira Partida](#12-primeira-partida)
13. [Auto-Tune e Ajuste Fino](#13-auto-tune-e-ajuste-fino)
14. [Data Logging](#14-data-logging)
15. [Troubleshooting](#15-troubleshooting)
16. [Recursos da Comunidade](#16-recursos-da-comunidade)

---

## 1. INTRODUÇÃO

### 1.1 O que é Speeduino?

**Speeduino** é um sistema de injeção eletrônica programável **open-source** baseado no Arduino, compatível com **TunerStudio** (software usado em ECUs como Megasquirt).

**Principais características:**
- 100% open-source (hardware + firmware)
- Custo 10x menor que Megasquirt
- Suporta até 8 cilindros sequenciais
- Ignição totalmente mapeada
- Boost control, launch control, flat shift
- Data logging em SD card
- Comunidade ativa (especialmente no Brasil)

### 1.2 TunerStudio

**TunerStudio MS** é o software de configuração e tuning usado para:
- Configurar parâmetros da ECU
- Criar e editar mapas (VE, ignição, AFR)
- Monitorar dados em tempo real
- Auto-tune (ajuste automático de VE)
- Data logging (registro de dados)

**Versões:**
- **TunerStudio MS Lite:** Gratuita (com limitações)
- **TunerStudio MS:** Paga (~$60 USD) - recomendada

**Requisitos mínimos:**
- TunerStudio 3.0.7+ (recomendado: última versão 3.2.05+)
- Windows, Mac ou Linux
- Porta USB disponível

### 1.3 Por que este guia?

Este documento foi criado especificamente para o **VW Gol AP 1.8** usando **SCG-ECU 2.0 (STM32F407)**, que tem diferenças importantes em relação ao Arduino Mega tradicional do Speeduino.

**Fontes consultadas:**
- Speeduino Manual oficial (wiki.speeduino.com)
- Fóruns Speeduino BR (speeduino.com/forum)
- STM32F407 development threads
- Manual Speeduino em Português (Scribd)
- Experiência com SCG-ECU code base

---

## 2. DIFERENÇAS STM32F407 VS ARDUINO

### 2.1 Hardware

| Aspecto | Arduino Mega 2560 | STM32F407VGT6 |
|---------|------------------|---------------|
| CPU | 8-bit AVR @ 16 MHz | 32-bit ARM @ 168 MHz |
| Flash | 256 KB | 1024 KB (1 MB) |
| RAM | 8 KB | 192 KB |
| Voltage | 5V | 3.3V ⚠️ |
| EEPROM | 4 KB | Emulado (Flash) |
| CAN Bus | Não | Sim (nativo) |
| RTC | Não | Sim |
| SD Card | Ext. | Nativo |

### 2.2 Diferenças Críticas para TunerStudio

#### ⚠️ **Tensão de Alimentação (CRÍTICO!)**
```
Arduino Mega: 5V
STM32F407:   3.3V

TPS DEVE SER ALIMENTADO POR 3.3V, NÃO 5V!
Sensores analógicos: 0-3.3V (não 0-5V)
```

#### **Storage (EEPROM)**
```
Arduino Mega: EEPROM nativa de 4KB
STM32F407:   Flash emulado (configurado em globals.h)

⚠️ Primeira gravação pode demorar mais!
⚠️ Após upload, sempre reset hard (power cycle)
```

#### **Baud Rate**
```
Arduino Mega: 115200 (padrão Speeduino)
STM32F407:   115200 (mesmo padrão)

✓ Sem mudanças necessárias no TunerStudio
```

#### **Pinout Differences**
```
Arduino Mega: Pinout fixo compatível com shields v0.3/v0.4
STM32F407:   Pinout customizado (definido em board_stm32_official.h)

⚠️ Não use shields Arduino diretamente!
⚠️ SCG-ECU tem pinout próprio (ver seção 3)
```

#### **USB Support**
```
Arduino Mega: USB-Serial via FTDI/16U2
STM32F407:   USB nativo (CDC - Communications Device Class)

Em Arduino IDE:
  Tools → USB support: "CDC (generic serial)"
```

### 2.3 Vantagens STM32F407

✅ **10x mais rápido** - resolução de timing muito melhor
✅ **CAN Bus nativo** - comunicação com outros módulos (BMW, VW)
✅ **SD Card logging** - data logging direto em cartão
✅ **Mais memória** - tabelas maiores, mais features
✅ **RTC** - timestamping preciso de logs

### 2.4 Configuração Arduino IDE (Referência)

Se precisar compilar firmware:

```
Tools → Board: "GENERIC STM32F4 series"
Tools → Board part number: "BLACK F407VE"
Tools → USART support: "Enabled (generic serial)"
Tools → USB support: "CDC (generic serial)"
Tools → Upload method: "STM32CubeProgrammer (DFU)"
```

**⚠️ Nota:** Para SCG-ECU, use PlatformIO (não Arduino IDE)!

---

## 3. PREPARAÇÃO DO HARDWARE

### 3.1 Pinout SCG-ECU (STM32F407VGT6)

#### **Entradas Analógicas (0-3.3V)**
```
PA0: TPS (0-3.3V) ⚠️ NÃO 5V!
PA1: MAP (0-3.3V ou freq)
PA2: CLT (NTC 2490Ω pullup)
PA3: IAT (NTC 2490Ω pullup)
PA4: O2/Wideband (0-3.3V)
PA5: Battery Voltage (divider 0-3.3V)
PA6: Oil Pressure (opcional)
PA7: Fuel Pressure (opcional)
```

#### **Entradas Digitais**
```
PB0: Crank (Hall/VR) - PRIMARY TRIGGER
PB1: Cam (Hall/VR) - SECONDARY TRIGGER (sequential)
PB2: Knock 1 (sensor detonação)
PB3: Knock 2 (opcional)
PB4: Clutch Switch (flat shift)
PB5: Brake Switch (opcional)
```

#### **Saídas Injeção (Low-side drivers)**
```
PC0: Injetor Cilindro 1
PC1: Injetor Cilindro 2
PC2: Injetor Cilindro 3
PC3: Injetor Cilindro 4
PC4: Injetor 5 (reserva)
PC5: Injetor 6 (reserva)
```

#### **Saídas Ignição (Logic level para módulos)**
```
PD0: Bobina 1
PD1: Bobina 2
PD2: Bobina 3
PD3: Bobina 4
```

#### **Saídas PWM/Auxiliares**
```
PE0: Idle Control (PWM valve ou stepper)
PE1: Boost Control (solenoid)
PE2: VVT (variador comando)
PE3: Fan Relay
PE4: Fuel Pump Relay
PE5: Tacho Output (sinal para painel)
PE6: Aux Output 1
PE7: Aux Output 2
```

#### **Comunicação**
```
PA9:  USART1_TX (debug serial)
PA10: USART1_RX
PA11: CAN1_RX (BMW/VW CAN)
PA12: CAN1_TX
USB:  Micro USB (TunerStudio connection)
```

### 3.2 Checklist de Conexões

**ANTES de ligar:**

```
□ TPS alimentado por 3.3V (não 5V!)
□ MAP conectado (analógico ou freq)
□ CLT conectado (NTC pullup 2490Ω)
□ IAT conectado (NTC pullup 2490Ω)
□ Wideband O2 conectado (LSU 4.9)
□ Battery voltage divider OK (12V → 3.3V)
□ Crank sensor conectado (PB0)
□ Cam sensor conectado se sequential (PB1)
□ Injetores 1-4 conectados (PC0-PC3)
□ Bobinas conectadas (PD0-PD3 ou PD0 se distribuidor)
□ Fuel pump relay (PE4)
□ Fan relay (PE3)
□ Idle valve (PE0)
□ Grounds OK (múltiplos grounds!)
□ Sem curtos (multímetro!)
```

### 3.3 Sensor MAP - VW Original vs Upgrade

#### **Opção 1: MAP Original VW (Frequência)**
```
Tipo: Frequency-based (Bosch 0 261 230 012)
Range: 80-162 Hz
  80 Hz  = 20 kPa (vácuo)
  100 Hz = 100 kPa (atmosfera)
  162 Hz = 300 kPa (2 bar boost)

TunerStudio config:
  MAP Type: FREQUENCY
  Min freq: 80 Hz
  Max freq: 162 Hz
```

#### **Opção 2: GM 3-Bar (Analógico) - RECOMENDADO**
```
Tipo: Analog voltage (GM 12223861)
Range: 0.5-4.5V
  0.5V = 0 kPa
  2.5V = 100 kPa (atmosfera)
  4.5V = 300 kPa (2 bar boost)

⚠️ Usar divisor de tensão se sensor é 5V:
  Vout = Vsensor × (R2 / (R1 + R2))
  Para 5V → 3.3V: R1=1kΩ, R2=2kΩ

TunerStudio config:
  MAP Type: ANALOG
  Min voltage: 0.5V
  Max voltage: 4.5V
```

**Recomendação:** GM 3-bar (mais preciso, resposta rápida, fácil config)

### 3.4 Wideband O2 - OBRIGATÓRIO!

**⚠️ NUNCA use narrowband (sensor de 1 fio original)!**

```
Sensor: LSU 4.9 (Bosch)
Controller: AEM, Innovate LC-2, Spartan, DIY CJ125

Output: 0-5V linear → dividir para 0-3.3V
  0.5V = Lambda 0.68 (rico)
  2.5V = Lambda 1.00 (estequiométrico)
  4.5V = Lambda 1.36 (pobre)

Divisor 5V → 3.3V:
  Vin ──┬──[R1:1kΩ]──┬── Vout (PA4)
        │            │
       5V         [R2:2kΩ]
                     │
                    GND

Wideband essencial para:
  ✓ Auto-tune VE table
  ✓ Tuning seguro
  ✓ Monitoramento real-time
```

---

## 4. INSTALAÇÃO DO TUNERSTUDIO

### 4.1 Download

**Site oficial:** https://www.tunerstudio.com/

**Versões:**
- **Windows:** TunerStudioMS-windows-installer.exe
- **Mac:** TunerStudioMS.dmg
- **Linux:** TunerStudioMS-linux.tar.gz

**Versão mínima:** 3.0.7
**Recomendado:** 3.2.05 ou mais recente (Nov 2024)

### 4.2 Instalação Windows

```
1. Baixar instalador
2. Executar como Administrador
3. Seguir wizard (Next → Next → Install)
4. Não precisa instalar drivers (STM32 usa CDC nativo)
5. Finish
```

### 4.3 Instalação de Drivers (se necessário)

**STM32F407 geralmente não precisa!** (CDC nativo)

Se Windows não reconhecer:
```
1. Baixar "STM32 Virtual COM Port Driver" (ST website)
2. Instalar
3. Reconectar USB
4. Verificar Device Manager → Ports (COM & LPT)
```

### 4.4 Preparar speeduino.ini

O arquivo `.ini` define a interface entre TunerStudio e Speeduino.

**Localização no projeto SCG-ECU:**
```
C:\Users\...\speeduino\reference\speeduino.ini
```

**Copiar para:**
```
C:\Users\<SeuUsuario>\Documents\TunerStudioProjects\
```

**Versão no .ini:**
```ini
[TunerStudio]
   iniSpecVersion = 3.64

[MegaTune]
   signature = "speeduino 202504-dev"
```

**⚠️ IMPORTANTE:** Use o `speeduino.ini` do SEU firmware, não de outras fontes!

---

## 5. PRIMEIRA CONEXÃO

### 5.1 Ligar SCG-ECU

```
1. Conectar USB (micro USB na SCG-ECU)
2. Ligar ignição do carro (ou fonte bancada 12V)
3. LED power deve acender
4. Windows detecta "STM32 Virtual COM Port"
5. Verificar Device Manager → número da porta (ex: COM3)
```

### 5.2 Abrir TunerStudio

```
1. Launch TunerStudio
2. Create New Project (primeira vez)
   ou
   File → New Project
```

### 5.3 New Project Wizard

#### **Passo 1: Project Name**
```
Project Name: GOL_AP18_SCG_ECU
Project Location: (deixar padrão)

[Next]
```

#### **Passo 2: Select ECU Type**
```
ECU Type: Speeduino

⚠️ Não selecionar "Megasquirt" ou outros!

[Next]
```

#### **Passo 3: Select INI File**
```
[Browse]
→ Navegar até: speeduino\reference\speeduino.ini
→ Select

[Next]
```

#### **Passo 4: Communication Settings**
```
Connection Type: Serial/USB Connection

Serial Port: COMx (selecionar a porta do STM32)
Baud Rate: 115200 ⚠️ CRÍTICO!

[Test Port] → deve retornar "Test successful"

Se falhar:
  - Verificar porta COM correta
  - Verificar cabo USB (alguns não têm data)
  - Verificar se ECU está alimentada (12V)
  - Verificar se firmware foi gravado

[Next]
```

#### **Passo 5: Load Base Tune (IMPORTANTE!)**
```
□ Load a base tune now

Se tiver tune pronto:
  [Browse] → selecionar .msq

Se primeira instalação:
  □ DEIXAR DESMARCADO
  (vamos configurar manualmente primeiro)

[Finish]
```

### 5.4 Primeira Comunicação

```
TunerStudio abre:
  - Dashboard vazio
  - Pode ter avisos de "No data"

Menu: Communications → Connect
  ou
  Botão toolbar: [Connect] (plug icon)

Status bar (bottom):
  "Connected to Speeduino" ✓

Se conectar OK:
  - Dashboard começa mostrar valores
  - RPM = 0
  - TPS = lendo valor
  - CLT = lendo temperatura
```

### 5.5 Verificar Firmware Version

```
Menu: Help → About Speeduino

Deve mostrar:
  Signature: speeduino 202504-dev
  Board: STM32 (ou similar)

Se mostrar "Arduino Mega":
  ⚠️ Firmware errado! Recompilar para STM32
```

---

## 6. CONFIGURAÇÃO ENGINE CONSTANTS

**Menu: Tuning → Engine Constants**

Vamos configurar TODOS os parâmetros fundamentais do motor AP 1.8.

### 6.1 Settings → Engine

```
Number of Cylinders: 4
Engine Type: EVEN_FIRE (firing order regular)
Firing Order: 1-3-4-2

  Cilindro #1 = Lado da polia
  Ordem: 1 → 3 → 4 → 2

Engine Stroke: FOUR_STROKE

Displacement: 1781 cc
  (1.8 litros = 1781 cm³)

Cylinders Layout: INLINE (em linha)
```

### 6.2 Settings → Injection

#### **Injection Settings**
```
Injection Mode: SEQUENTIAL
  (cada cilindro injeta no tempo certo)

  Alternativas:
    SEMI-SEQUENTIAL: injeta 2 por vez (mais simples)
    SIMULTANEOUS: todos juntos (básico)

Number of Injectors: 4

Injection Staging: OFF
  (sem injetores secundários)
```

#### **Injector Specifications**
```
ASPIRADO:
  Injector Flow Rate: 280 cc/min
  Rated At Pressure: 3.0 bar (43.5 psi)
  Injector Dead Time: 1.0 ms @ 14.0V

TURBO:
  Injector Flow Rate: 440 cc/min
  Rated At Pressure: 3.0 bar
  Injector Dead Time: 0.95 ms @ 14.0V

⚠️ Dead time depende do injetor específico!
   Consultar datasheet ou ajustar depois.
```

#### **Injector Characteristics (Dead Time Table)**
```
Voltage (V)  Dead Time (ms) - BOSCH 280cc
─────────────────────────────────────
   10.0          1.35
   11.0          1.20
   12.0          1.10
   13.0          1.05
   14.0          1.00
   15.0          0.98
   16.0          0.95

⚠️ Ajustar conforme seu injetor!
```

### 6.3 Settings → Ignition

#### **Ignition Mode - ASPIRADO (Distribuidor)**
```
Ignition Mode: WASTED_SPARK_DISTRIBUTOR

Number of Ignition Outputs: 1
  (1 saída para distribuidor Hall)

Coil Type: SINGLE_COIL

Distributor: YES

Spark Output: GOING_HIGH
  (some coils trigger on falling edge)
```

#### **Ignition Mode - TURBO (Coil-on-Plug)**
```
Ignition Mode: SEQUENTIAL

Number of Ignition Outputs: 4

Coil Type: INDIVIDUAL_COILS

Spark Output: GOING_LOW
  (IGBTs geralmente ativam em LOW)
```

#### **Dwell Settings**
```
Dwell Control: VOLTAGE_BASED

Dwell Table:
Voltage (V)  Dwell (ms) - Bobina padrão
────────────────────────────────────
   10.0         4.5
   11.0         4.0
   12.0         3.5
   13.0         3.2
   14.0         3.0
   15.0         2.9
   16.0         2.8

⚠️ Turbo com coil-on-plug: +0.5ms (arco mais forte)

Cranking Dwell: 5.0 ms
  (mais tempo para arco forte na partida)
```

### 6.4 Settings → Rev Limits

```
ASPIRADO:
  RPM Soft Limit: 6000 RPM
  RPM Hard Limit: 6500 RPM

  Limiter Type: FUEL_CUT
    (corta combustível, mantém ignição)

TURBO:
  RPM Soft Limit: 6500 RPM
  RPM Hard Limit: 7000 RPM

  Limiter Type: FUEL_AND_SPARK
    (corta ambos - mais seguro)
```

### 6.5 Settings → Algorithm

```
Load Source: MAP
  (baseado em pressão do coletor)

  Alternativa: TPS (Alpha-N, sem MAP)

Fuel Algorithm: SPEED_DENSITY

Barocorrection: ENABLED
  (correção por altitude/pressão atmosférica)
```

### 6.6 Gravar Configuração

**⚠️ SEMPRE gravar após mudanças!**

```
Toolbar: [Burn] (ícone chama)
  ou
Keyboard: Ctrl+B
  ou
Menu: Tools → Burn to ECU

Aguardar: "Burn Complete!"

⚠️ STM32: Primeira gravação pode demorar ~10 segundos
   (escrevendo Flash emulado)
```

---

## 7. TRIGGER SETUP

**Menu: Tuning → Trigger Setup**

Configuração do sensor de rotação (crank) e fase (cam).

### 7.1 Opção 1: Distribuidor Hall Original VW

**Configuração mais simples** - funciona com distribuidor original Gol.

```
╔══════════════════════════════════════╗
║ PRIMARY TRIGGER                      ║
╠══════════════════════════════════════╣
  Trigger Pattern: BASIC_DISTRIBUTOR

  Primary Trigger Speed: CAM_SPEED
    (distribuidor gira a metade do virabrequim)

  Primary Trigger Edge: RISING
    (quando sinal sobe 0→1)

  Missing Teeth: 0

  Trigger Teeth: 4
    (1 pulso por cilindro = 4 pulsos/revolução distribuidor)

╔══════════════════════════════════════╗
║ SECONDARY TRIGGER                    ║
╠══════════════════════════════════════╣
  Secondary Trigger: DISABLED
    (distribuidor não tem cam sensor)

╔══════════════════════════════════════╗
║ TRIGGER ANGLE                        ║
╠══════════════════════════════════════╣
  Trigger Angle: 0° ATDC
    (ajustar depois com timing light)

  Trigger Angle Multiplier: 1
```

**Conexão física:**
```
Distribuidor Hall VW:
  Pino 1: 12V (ignição)
  Pino 2: Ground
  Pino 3: Signal → SCG-ECU PB0 (crank input)

⚠️ Verificar se Hall precisa pullup 5V ou 12V!
```

**Limitações:**
- Resolução baixa (só 4 pulsos/rev)
- Não é sequential (sem cam sync)
- Timing accuracy ~5-10°

**Vantagens:**
- Simples de instalar
- Usa distribuidor original
- Confiável

### 7.2 Opção 2: Roda Fônica 60-2 (UPGRADE)

**Configuração avançada** - requer instalação de roda fônica.

```
╔══════════════════════════════════════╗
║ PRIMARY TRIGGER                      ║
╠══════════════════════════════════════╣
  Trigger Pattern: MISSING_TOOTH

  Primary Trigger Speed: CRANK_SPEED
    (roda no virabrequim)

  Primary Trigger Edge: RISING
    (Hall) ou FALLING (VR sensor)

  Number of Teeth: 60

  Missing Teeth: 2
    (gap de 2 dentes para sincronismo)

  Trigger Angle: 90° BTDC
    ⚠️ AJUSTAR com timing light!
    (depende da posição física da roda)

╔══════════════════════════════════════╗
║ SECONDARY TRIGGER (Cam Sensor)      ║
╠══════════════════════════════════════╣
  Secondary Trigger: ENABLED

  Trigger Pattern: SINGLE_TOOTH
    (1 pulso por revolução do comando)

  Trigger Edge: RISING

  ✓ Habilita injeção SEQUENTIAL
```

**Conexão física:**
```
Sensor Crank (Hall ou VR):
  VR: 2 fios (AC signal)
    Positivo → PB0
    Negativo → Ground

  Hall: 3 fios (DC signal)
    VCC → 12V ou 5V (verificar sensor)
    Signal → PB0
    Ground → Ground

Sensor Cam (Hall):
  VCC → 12V/5V
  Signal → PB1
  Ground → Ground
```

**Vantagens:**
- Resolução alta (6° por dente)
- Timing accuracy <1°
- Sequential injection (com cam)
- Melhor resposta throttle
- Melhor idle stability

**Desvantagens:**
- Requer usinagem (roda + sensor)
- Instalação complexa
- Ajuste crítico (trigger angle)

### 7.3 Calibração Trigger Angle

**⚠️ CRÍTICO para timing correto!**

#### **Método 1: Timing Light (Preciso)**
```
1. Configurar Advance Table = 10° BTDC everywhere
2. Warm up motor (80°C)
3. Idle estável (850 RPM)
4. Conectar timing light no cilindro #1
5. Apontar para polia/volante
6. Observar marca de tempo

Se marca NÃO está em 10° BTDC:
  - Aumentar Trigger Angle se marca está ATRASADA
  - Diminuir Trigger Angle se marca está ADIANTADA

Repetir até marca = 10° exato

7. Conferir em várias RPMs (2000, 3000, 4000)
```

#### **Método 2: Tooth Logger (TunerStudio)**
```
Tools → Tooth Logger

1. Cranking (não ligar motor)
2. Start Logging
3. Cranking ~5 segundos
4. Stop Logging

Analisar:
  - Tooth timing uniforme? ✓
  - Missing tooth detectado? ✓
  - Sync loss? ✗ (não pode ter)

Se tiver erros:
  - Verificar sensor gap (Hall: 0.5-1mm, VR: 1-2mm)
  - Verificar fiação (shield/ground)
  - Verificar trigger edge (rising/falling)
```

### 7.4 Problemas Comuns

**Sintoma: Não sincroniza (no RPM reading)**
```
Causas:
  □ Sensor desconectado
  □ Trigger pattern errado
  □ Trigger edge errado (rising vs falling)
  □ Missing teeth configurado errado
  □ Sensor com gap incorreto
  □ Fiação com ruído (sem shield)
```

**Sintoma: RPM lê mas errático**
```
Causas:
  □ Sensor muito longe (gap grande)
  □ Ruído elétrico (velas/bobina perto do sensor)
  □ Roda fônica mal instalada (dentes irregulares)
  □ VR sensor sem resistor de pull-up/down
```

**Sintoma: Timing errado (marca não bate)**
```
Causas:
  □ Trigger angle incorreto
  □ Roda fônica instalada em posição errada
  □ Cam sensor invertido (sequential)
```

---

## 8. CONFIGURAÇÃO DE SENSORES

**Menu: Tuning → Sensors**

### 8.1 TPS (Throttle Position Sensor)

```
╔══════════════════════════════════════╗
║ TPS Configuration                    ║
╠══════════════════════════════════════╣
  TPS Type: POTENTIOMETER

  Input Pin: PA0

  Voltage Range: 0-3.3V ⚠️
    (STM32 é 3.3V, não 5V!)

  TPS Minimum (closed): 0.5V (ajustar na calibração)
  TPS Maximum (WOT): 3.0V (ajustar na calibração)

  TPS Filter: 50%
    (suavização de leitura)
```

**Conexão TPS original VW:**
```
Pino 1: Ground → GND
Pino 2: Signal → PA0
Pino 3: VCC → 3.3V ⚠️ NÃO 5V!

⚠️ Se TPS original é 5V:
   Usar divisor resistivo:
     TPS Signal ─┬─ [1kΩ] ─┬─ PA0
                 5V        └─ [2kΩ] ─ GND
```

### 8.2 CLT (Coolant Temperature)

```
╔══════════════════════════════════════╗
║ CLT Configuration                    ║
╠══════════════════════════════════════╣
  CLT Type: NTC_THERMISTOR

  Input Pin: PA2

  Pullup Resistor: 2490 Ω
    (verificar schematic SCG-ECU)

  Sensor Curve: GM/BOSCH (padrão VW)

  Temperature Range: -40°C to 120°C
```

**Curva NTC padrão VW:**
```
Temp (°C)   Resistance (Ω)
──────────────────────────
   -40        100,000
   -20         35,000
     0          9,500
    20          3,300
    40          1,500
    60            700
    80            320
   100            185
   120            120
```

**Conexão:**
```
Sensor NTC (2 fios):
  Fio 1: PA2 (SCG-ECU)
  Fio 2: Ground

Pullup interno SCG-ECU: 2490Ω para 3.3V
```

### 8.3 IAT (Intake Air Temperature)

```
╔══════════════════════════════════════╗
║ IAT Configuration                    ║
╠══════════════════════════════════════╣
  IAT Type: NTC_THERMISTOR

  Input Pin: PA3

  Pullup Resistor: 2490 Ω

  Sensor Curve: GM/BOSCH (mesma do CLT)

  Temperature Range: -40°C to 80°C
```

**Instalação IAT:**
```
Local: Coletor de admissão (após throttle body)

Evitar:
  ✗ Antes do throttle (lê temperatura ambiente)
  ✗ Muito perto do motor (heat soak)
  ✗ Em curva de tubo (fluxo irregular)

Ideal:
  ✓ Trecho reto após borboleta
  ✓ Fluxo de ar direto no sensor
  ✓ Longe de fontes de calor
```

### 8.4 MAP (Manifold Absolute Pressure)

#### **Opção A: MAP Analógico (GM 3-bar)**

```
╔══════════════════════════════════════╗
║ MAP Configuration - Analog           ║
╠══════════════════════════════════════╣
  MAP Type: ANALOG

  Input Pin: PA1

  Voltage Range: 0.5V - 4.5V

  Pressure Range: 0 - 300 kPa

  Calibration:
    0.5V = 0 kPa
    2.5V = 100 kPa (atmosfera)
    4.5V = 300 kPa (2 bar boost)

  MAP Filter: 20%
```

**Conexão GM 3-bar:**
```
Pino 1: Ground → GND
Pino 2: VCC → 3.3V (ou 5V com divisor)
Pino 3: Signal → PA1

Se sensor é 5V:
  Signal ─┬─ [1kΩ] ─┬─ PA1
          5V       └─ [2kΩ] ─ GND
```

#### **Opção B: MAP Frequência (VW Original)**

```
╔══════════════════════════════════════╗
║ MAP Configuration - Frequency        ║
╠══════════════════════════════════════╣
  MAP Type: FREQUENCY

  Input Pin: PA1 (freq input capable)

  Frequency Range: 80 - 162 Hz

  Pressure Range: 20 - 300 kPa

  Calibration:
    80 Hz = 20 kPa
    100 Hz = 100 kPa
    162 Hz = 300 kPa
```

**Conexão MAP freq VW:**
```
Pino 1: Ground → GND
Pino 2: 12V (ignição) → VBAT
Pino 3: Signal → PA1
Pino 4: Referência → Atmosfera
```

### 8.5 O2 / Lambda (Wideband)

```
╔══════════════════════════════════════╗
║ O2 Configuration - Wideband         ║
╠══════════════════════════════════════╣
  O2 Type: WIDEBAND_LINEAR

  Input Pin: PA4

  Voltage Range: 0.5V - 4.5V

  Lambda Range: 0.68 - 1.36

  Calibration:
    0.5V = Lambda 0.68 (rich)
    2.5V = Lambda 1.00 (stoich)
    4.5V = Lambda 1.36 (lean)

  Warmup Delay: 60 seconds
    (sensor precisa aquecer antes de usar)
```

**Controllers wideband compatíveis:**
- AEM UEGO
- Innovate LC-2
- Spartan 2
- DIY CJ125

**Conexão:**
```
Controller Output (0-5V):
  Signal ─┬─ [1kΩ] ─┬─ PA4
         5V        └─ [2kΩ] ─ GND

Ground: Comum com SCG-ECU
```

### 8.6 Battery Voltage

```
╔══════════════════════════════════════╗
║ Battery Voltage                      ║
╠══════════════════════════════════════╣
  Input Pin: PA5

  Voltage Divider:
    R1: 10kΩ (high side)
    R2: 2.2kΩ (low side)

  Range: 0-18V → 0-3.3V

  Calibration:
    Measure real voltage
    Adjust multiplier in TunerStudio
```

**Cálculo divisor:**
```
Vout = Vin × (R2 / (R1 + R2))

12V × (2.2k / 12.2k) = 2.16V ✓
```

---

## 9. CALIBRAÇÃO DE SENSORES

### 9.1 Calibrar TPS

**Menu: Tools → Calibrate TPS**

```
╔══════════════════════════════════════╗
║ TPS Calibration Wizard               ║
╠══════════════════════════════════════╣

1. Ligar ignição (motor OFF)

2. Acelerador completamente FECHADO
   [Read Closed Value]

   Deve ler: ~0.5V (15%)

   Se fora: Ajustar TPS fisicamente!

3. Acelerador completamente ABERTO (WOT)
   [Read Open Value]

   Deve ler: ~3.0V (90%)

   ⚠️ Pisar FUNDO no acelerador!

4. [Apply Calibration]

5. Verificar:
   - Closed = 0%
   - WOT = 100%
   - Movimento suave entre 0-100%
```

**Troubleshooting TPS:**
```
Problema: Leitura instável (pulando)
Solução:
  - Verificar conexões (oxidação)
  - Aumentar TPS Filter para 70%
  - Substituir TPS se desgastado

Problema: Não chega 100% WOT
Solução:
  - Ajustar cabo acelerador
  - Ajustar batente TPS
  - Verificar se throttle abre totalmente

Problema: Leitura >3.3V
Solução:
  ⚠️ TPS alimentado por 5V!
  - Desligar IMEDIATAMENTE
  - Mudar alimentação para 3.3V
  - Verificar se PA0 não queimou
```

### 9.2 Verificar CLT

**Dashboard → CLT Reading**

```
Motor frio (ambiente):
  Leitura deve ser ~20-25°C

Se lê -40°C:
  ⚠️ Sensor desconectado (lê pullup)

Se lê 120°C+:
  ⚠️ Sensor em curto (lê 0Ω)

Motor quente (funcionando):
  Leitura deve subir gradualmente
  Normal: 80-95°C
  Termostato abre: ~87°C
```

**Teste bancada:**
```
1. Desconectar sensor
2. Medir resistência com multímetro

   20°C ambiente: ~3300Ω (3.3kΩ)

3. Conectar resistor conhecido:
   - 3.3kΩ = ~20°C
   - 300Ω = ~80°C

4. Verificar TunerStudio lê correto
```

### 9.3 Verificar MAP

**Dashboard → MAP Reading**

```
Motor OFF, ignição ON:
  Leitura: ~100 kPa (pressão atmosférica)

  ⚠️ Varia com altitude:
    Nível do mar: 101 kPa
    São Paulo (750m): 93 kPa
    Belo Horizonte (850m): 91 kPa

Motor IDLE (sem vácuo leak):
  Aspirado: 40-60 kPa

  Vácuo alto: 30-40 kPa (boa vedação)
  Vácuo baixo: 70-80 kPa (leak ou overlap alto)

Motor WOT (acelerador fundo):
  Aspirado: 95-100 kPa (atmosférico)
  Turbo: 100+ kPa (boost)
```

**Teste vacuum leak:**
```
1. Motor em idle quente
2. Tampar entrada ar filtro (luva)
3. MAP deve cair para <20 kPa
4. Soltar → deve voltar para 40-60 kPa

Se não responde:
  ⚠️ Sensor ruim ou mangueira desconectada
```

### 9.4 Verificar Lambda

**Dashboard → Lambda Reading**

```
⚠️ Aguardar warmup 60 segundos!

Motor idle quente:
  Lambda target: 1.00 (14.7:1)
  Leitura: 0.95-1.05 (aceitável)

Motor WOT:
  Lambda target: 0.85 (12.5:1)
  Leitura: 0.82-0.88 (rico, seguro)

Se lê 0.00:
  - Sensor ainda aquecendo
  - Controller desligado
  - Fio desconectado

Se lê sempre 1.00:
  - Controller em modo narrowband
  - Configuração errada
```

---

## 10. TABELAS DE MAPEAMENTO

### 10.1 VE Table (Volumetric Efficiency)

**Menu: Tuning → VE Table 1**

**O que é VE?**
```
VE = Eficiência volumétrica do motor
   = Quanto ar realmente entra vs. teórico

VE 100% = Motor "respira" perfeitamente
VE 85% = Motor típico aspirado (intake restriction)
VE 110% = Turbo com boost (mais ar que atmosférico)
```

**Tabela base CONSERVADORA (Aspirado):**

```
       RPM →
MAP ↓   850   1500   2000   2500   3000   3500   4000   4500   5000   5500   6000
────────────────────────────────────────────────────────────────────────────────────
20 kPa   60    62     65     67     68     69     70     69     68     66     64
30 kPa   65    67     70     72     74     75     76     75     73     71     68
40 kPa   68    70     73     76     78     80     82     81     79     76     72
50 kPa   70    73     76     79     82     84     86     85     83     80     76
60 kPa   72    75     79     82     85     88     90     89     86     83     79
70 kPa   73    77     82     85     88     91     92     91     89     85     81
80 kPa   75    79     84     87     90     92     93     92     90     86     82
90 kPa   75    81     86     89     91     93     93     92     91     88     84
100 kPa  75    83     88     91     92     93     93     92     91     89     86
```

**Como usar:**
```
1. Carregar tabela base
2. [Burn to ECU]
3. Auto-tune depois (próxima seção)
```

**Valores típicos:**
```
Idle (850 RPM, 40 kPa): 68-75%
Cruise (2500 RPM, 40 kPa): 76-82%
WOT (4000 RPM, 100 kPa): 90-93%
Peak torque (3000 RPM): 93-95%
```

### 10.2 Ignition Table (Advance)

**Menu: Tuning → Spark Table**

**⚠️ VALORES CONSERVADORES PARA GASOLINA 91+ OCTANAS**

```
       RPM →
MAP ↓   850   1500   2000   2500   3000   3500   4000   4500   5000   5500   6000
────────────────────────────────────────────────────────────────────────────────────
20 kPa   12°   20°    22°    24°    25°    25°    25°    24°    24°    23°    22°
30 kPa   12°   19°    21°    23°    24°    24°    24°    23°    23°    22°    21°
40 kPa   11°   18°    20°    22°    23°    23°    23°    22°    22°    21°    20°
50 kPa   11°   17°    19°    21°    22°    22°    22°    21°    21°    20°    19°
60 kPa   10°   16°    18°    20°    21°    21°    21°    20°    20°    19°    18°
70 kPa   10°   15°    17°    19°    20°    20°    20°    19°    19°    18°    17°
80 kPa   10°   14°    16°    18°    19°    19°    19°    18°    18°    17°    16°
90 kPa   10°   13°    15°    17°    18°    18°    18°    17°    17°    16°    15°
100 kPa  10°   12°    14°    16°    17°    17°    17°    16°    16°    15°    14°
```

**⚠️ ATENÇÃO CRÍTICA:**
```
COMEÇAR SEMPRE COM VALORES CONSERVADORES!

Gasolina comum (87 oct): -2° de tudo acima
Etanol (E85-E100): +3-5° (octanagem mais alta)

Detonação = MOTOR DESTRUÍDO!

Sinais de detonação:
  ✗ Barulho "pedregulho na lata"
  ✗ Perda de potência
  ✗ EGT subindo rápido
  ✗ Knock sensor ativando

Se detonar:
  1. REDUZIR -5° IMEDIATAMENTE
  2. Enriquecer lambda -0.05
  3. Trocar combustível
```

**Turbo boost retard:**
```
Por cada 0.1 bar de boost: -1 a -2° advance

Exemplo 1.0 bar boost (100 kPa sobre atmosférico):
  Base: 17° @ 4000 RPM
  Boost: -10 a -12°
  Final: 5-7° @ 4000 RPM, 200 kPa MAP
```

### 10.3 AFR/Lambda Target Table

**Menu: Tuning → AFR Table**

**Lambda targets (Gasolina):**

```
       RPM →
MAP ↓   850   1500   2000   2500   3000   3500   4000   4500   5000   5500   6000
────────────────────────────────────────────────────────────────────────────────────
20 kPa  1.00  1.05   1.05   1.05   1.05   1.05   1.05   1.05   1.03   1.00   1.00
30 kPa  1.00  1.03   1.03   1.03   1.03   1.03   1.02   1.00   1.00   0.98   0.95
40 kPa  1.00  1.02   1.02   1.02   1.00   1.00   0.98   0.95   0.95   0.92   0.90
50 kPa  1.00  1.00   1.00   1.00   0.98   0.95   0.92   0.90   0.90   0.88   0.87
60 kPa  1.00  0.98   0.98   0.95   0.92   0.90   0.88   0.87   0.87   0.86   0.86
70 kPa  0.98  0.95   0.95   0.92   0.90   0.88   0.86   0.86   0.85   0.85   0.85
80 kPa  0.95  0.92   0.92   0.90   0.88   0.86   0.85   0.85   0.85   0.85   0.85
90 kPa  0.92  0.90   0.90   0.88   0.86   0.85   0.85   0.85   0.85   0.85   0.85
100 kPa 0.90  0.88   0.88   0.86   0.85   0.85   0.85   0.85   0.85   0.85   0.85
```

**Conversão Lambda ↔ AFR:**
```
Gasolina (stoich = 14.7):
  Lambda 1.00 = 14.7:1 AFR
  Lambda 0.85 = 12.5:1 AFR (rico, WOT)
  Lambda 1.05 = 15.4:1 AFR (pobre, cruise)

Etanol (stoich = 9.0):
  Lambda 1.00 = 9.0:1 AFR
  Lambda 0.85 = 7.7:1 AFR (rico, WOT)
  Lambda 1.05 = 9.5:1 AFR (pobre, cruise)
```

**Estratégia AFR:**
```
Cruise (baixa carga): Lambda 1.03-1.05
  → Economia de combustível

Medium load: Lambda 1.00
  → Resposta boa, consumo OK

WOT (alta carga): Lambda 0.85-0.88
  → Potência máxima + segurança
  → Resfriamento por combustível

⚠️ NUNCA Lambda >1.10 em WOT!
   (detonation + EGT alto = morte do motor)
```

### 10.4 Accel Enrichment

**Menu: Tuning → Accel Wizard**

```
╔══════════════════════════════════════╗
║ Acceleration Enrichment              ║
╠══════════════════════════════════════╣

TPS-based (Throttle delta):

  TPS Rate (%/s)   Enrichment (%)
  ──────────────────────────────
       10               25
       20               40
       30               60
       50              100
       75              130
      100              150

Decay Time: 500 ms
  (tempo para enrichment voltar a 0)

MAP-based (Pressure delta):
  DISABLED (usar TPS é suficiente)
```

**O que faz:**
```
Detecta aceleração rápida (TPS subindo rápido)
→ Injeta combustível extra (pump shot)
→ Evita hesitação / lean spike

Ajustar se:
  - Hesita ao acelerar: aumentar amount
  - Falha/corta: aumentar amount
  - Afoga: diminuir amount
  - Demora voltar: diminuir decay time
```

### 10.5 Warmup Enrichment (WUE)

**Menu: Tuning → Warmup Enrichment**

```
Temp (°C)   Enrichment (%)
──────────────────────────
   -20         200
   -10         180
     0         150
    10         135
    20         125
    30         115
    40         110
    50         105
    60         102
    70         101
    80         100 ← Normal operating temp
    90         100
```

**Como funciona:**
```
Motor frio precisa mais combustível (evaporação ruim)

CLT < 60°C: WUE ativo
CLT > 80°C: WUE = 100% (sem enrichment)

Se não pega a frio:
  → Aumentar WUE @ 0-20°C para 180-200%

Se afoga a frio:
  → Diminuir WUE @ 0-20°C para 130-150%
```

### 10.6 Afterstart Enrichment (ASE)

**Menu: Tuning → Afterstart Enrichment**

```
Time (s)    Enrichment (%)
───────────────────────────
    0           150
    1           140
    2           120
    3           110
    5           100
```

**Como funciona:**
```
Após partida bem-sucedida (RPM > 400):
  → Injeta combustível extra por alguns segundos
  → Estabiliza idle inicial
  → Compensa cilindros irregulares

Ajustar se:
  - Morre logo após pegar: aumentar amount/time
  - Roda irregular 3-5s: aumentar amount
  - Fumaça preta inicial: diminuir amount
```

---

## 11. PROTEÇÕES E LIMITES

**Menu: Tuning → Engine Protection**

### 11.1 Rev Limiter

```
╔══════════════════════════════════════╗
║ Rev Limiter                          ║
╠══════════════════════════════════════╣

ASPIRADO:
  Soft Limit: 6000 RPM
  Hard Limit: 6500 RPM

TURBO:
  Soft Limit: 6500 RPM
  Hard Limit: 7000 RPM

Limiter Type: FUEL_CUT
  (corta injetores, mantém ignição)

Cut Method: ROLLING
  (corta cilindros alternados, smooth)
```

### 11.2 Coolant Protection

```
╔══════════════════════════════════════╗
║ Coolant Temperature Protection       ║
╠══════════════════════════════════════╣

Warning Temp: 105°C
  → Acende luz no painel
  → Logging intensifica

Limp Mode Temp: 110°C
  → Corta 30% da potência
  → Rev limit 4000 RPM
  → Enriquece lambda +10%

Fan Control:
  Fan ON: 92°C
  Fan OFF: 85°C
  Fan Pin: PE3
```

### 11.3 Fuel Cut on Decel (DFCO)

```
╔══════════════════════════════════════╗
║ Deceleration Fuel Cut Off            ║
╠══════════════════════════════════════╣

Activate Conditions:
  RPM > 1800
  TPS < 2% (acelerador fechado)
  CLT > 60°C (motor quente)

Reactivate Fuel:
  RPM < 1500
  ou TPS > 5%

Vantagens:
  ✓ Economia combustível
  ✓ Motor brake
  ✓ Menos emissões
```

### 11.4 Idle Control

```
╔══════════════════════════════════════╗
║ Idle Control (IAC)                   ║
╠══════════════════════════════════════╣

Target RPM: 850

Algorithm: CLOSED_LOOP

PID Gains:
  P: 2.0
  I: 0.5
  D: 0.1

PWM Duty Range:
  Min: 10%
  Max: 90%

Output Pin: PE0

Idle-up Conditions:
  AC ON: +100 RPM
  Headlights ON: +50 RPM
  CLT < 60°C: +150 RPM
```

### 11.5 Boost Control (Turbo)

```
╔══════════════════════════════════════╗
║ Boost Control                        ║
╠══════════════════════════════════════╣

Max Boost: 110 kPa (1.0 bar gauge)
  ⚠️ Hard limit - nunca ultrapassar

Overboost Protection:
  Cut at: 120 kPa
  Cut type: FUEL_AND_SPARK
  Recovery: Manual reset

PID Control:
  P: 2.5
  I: 0.8
  D: 0.1

Slew Rate: 10 kPa/s
  (limite de subida de boost)
```

---

## 12. PRIMEIRA PARTIDA

### 12.1 Checklist PRÉ-Partida

**Elétrica:**
```
□ Battery 12-14V
□ Grounds apertados (múltiplos pontos)
□ Sem curtos (multímetro continuity test)
□ Fusíveis OK
□ Fuel pump liga (3 segundos ignição ON)
□ Injetores clicam (modo teste)
□ Bobinas faiscam (modo teste)
```

**Mecânica:**
```
□ Timing belt/chain alinhado
□ Oil level OK (verificar vareta)
□ Coolant level OK
□ Sem fuel leaks (cheirar ao redor)
□ Sem vacuum leaks (smoke test se possível)
□ Air filter instalado
□ Throttle abre/fecha livremente
```

**ECU:**
```
□ Config carregada (Burn to ECU ✓)
□ TPS calibrado (0% closed, 100% WOT)
□ MAP lendo ~100 kPa (ignição ON)
□ CLT lendo temp ambiente
□ Wideband aquecendo (luz piscando)
□ Trigger lendo (girar motor com chave)
```

### 12.2 Teste de Componentes

**Fuel Pump Test:**
```
Menu: Tools → Hardware Test

1. Select: Fuel Pump
2. [Activate]
   → Deve ouvir bomba ligando
3. Verificar pressão (manômetro): 3.0 bar
4. [Deactivate]
```

**Injector Test:**
```
Menu: Tools → Hardware Test

1. Select: Injector 1
2. [Activate]
   → Deve ouvir "click"
3. Repetir para injectors 2, 3, 4
4. Sem fuel leak?
```

**Ignition Test (CUIDADO!):**
```
⚠️ CUIDADO: ALTA TENSÃO!

Menu: Tools → Hardware Test

1. Remover vela cilindro 1
2. Conectar vela na bobina
3. Encostar vela no bloco (ground)
4. Select: Ignition 1
5. [Pulse]
   → Deve ver faísca azul forte
6. ⚠️ NÃO tocar!
```

### 12.3 Procedimento Primeira Partida

**Configurar Dashboard:**
```
Gauges visíveis:
  ✓ RPM (grande)
  ✓ TPS
  ✓ MAP
  ✓ CLT
  ✓ Lambda
  ✓ Advance
```

**Passo-a-passo:**

```
1. Ligar ignição (não dar partida ainda)
   - Fuel pump prime (3 segundos)
   - Dashboard TunerStudio conectado
   - Verificar:
     TPS = 0%
     MAP = ~100 kPa
     CLT = ambiente

2. Aguardar wideband aquecer (60 segundos)
   - Lambda controller: luz fixa (não piscando)

3. Primeira tentativa:
   - NÃO pisar no acelerador!
   - Pisar embreagem (se manual)
   - Girar chave (cranking)
   - Observar RPM no TunerStudio
   - Deve ler 200-400 RPM enquanto gira

4. Motor deve pegar em 2-10 segundos

5. SE PEGAR:
   → Vá para seção 12.4 (Após Pegar)

6. SE NÃO PEGAR:
   → Vá para seção 12.5 (Troubleshooting)
```

### 12.4 Após Pegar

**Observar imediatamente:**
```
✓ RPM estabiliza?
  Deve ir para 850-1200 RPM (cold idle up)

✓ Lambda correto?
  Cold: 0.95-1.05 (pouco rico OK)

✓ CLT subindo?
  Deve subir 1-2°C por minuto

✓ Oil pressure OK?
  >1 bar @ idle (se tiver gauge)

✓ Sem ruídos estranhos?
  Batidas, chiados, detonação

✓ Sem leaks?
  Combustível, água, vacuum
```

**Warm-up (5-10 minutos):**
```
Deixar em idle até:
  CLT > 80°C
  Lambda estabiliza em 1.00
  Idle RPM cai para 850

⚠️ NÃO acelerar com motor frio!
⚠️ NÃO sair dirigindo ainda!
```

**Verificar timing (timing light):**
```
1. Motor quente, idle estável
2. Advance table = 10° everywhere (temporário)
3. Conectar timing light cilindro #1
4. Apontar para polia/volante
5. Marca deve estar em 10° BTDC

Se NÃO está em 10°:
  → Ajustar Trigger Angle (seção 7.3)
```

### 12.5 Troubleshooting Partida

**Sintoma: RPM não lê (TunerStudio mostra 0)**
```
Causas:
  □ Crank sensor desconectado
  □ Trigger pattern errado
  □ Trigger edge errado (rising vs falling)
  □ Sensor muito longe (gap)

Verificar:
  1. Voltar para Trigger Setup (seção 7)
  2. Tools → Tooth Logger
  3. Cranking → deve ver pulsos
```

**Sintoma: RPM lê mas não pega**
```
Causas:
  □ Sem faísca (ignition)
  □ Sem combustível (injector/pump)
  □ Timing errado (muito avançado/atrasado)
  □ VE table muito baixa

Testar:
  1. Hardware Test → Injector (deve clicar)
  2. Hardware Test → Ignition (deve faiscar)
  3. Aumentar Cranking Enrichment 150-180%
  4. Verificar pressão combustível (3 bar)
```

**Sintoma: Pega e morre imediatamente**
```
Causas:
  □ Afterstart enrichment muito baixo
  □ Idle control não funcionando
  □ Vacuum leak
  □ MAP lendo errado

Solução:
  1. Aumentar ASE para 150% por 5 segundos
  2. Aumentar Idle target para 1000 RPM (temporário)
  3. Smoke test (procurar vacuum leaks)
```

**Sintoma: Roda muito irregular/falha**
```
Causas:
  □ Timing errado
  □ Injector morto
  □ Bobina morta
  □ Vacuum leak

Diagnóstico:
  1. Remover velas → qual está seca? (sem combustível)
  2. Teste de compressão
  3. Verificar resistência bobinas
  4. Verificar resistência injetores
```

**Sintoma: Fumaça preta (afogando)**
```
Causas:
  □ VE table muito alta
  □ WUE muito alto
  □ Injector preso aberto
  □ Pressure regulator quebrado (muita pressão)

Solução:
  1. Reduzir VE table -10% everywhere
  2. Reduzir WUE @ cold temps
  3. Verificar velas (pretas/molhadas?)
  4. Verificar pressão combustível (deve ser 3.0 bar)
```

---

## 13. AUTO-TUNE E AJUSTE FINO

### 13.1 Preparação Auto-Tune

**Requisitos:**
```
✓ Motor funcionando e quente (CLT > 80°C)
✓ Wideband lambda funcional
✓ Sem erros de sensores
✓ VE table base carregada
✓ Bateria boa (>12.5V)
✓ Combustível suficiente (tanque >50%)
```

### 13.2 Auto-Tune VE Table

**Menu: Tuning → VE Table**

```
╔══════════════════════════════════════╗
║ VE Analyze Live                      ║
╠══════════════════════════════════════╣

1. [Enable VE Analyze] (botão toggle ON)

2. Dashboard mostra:
   - VE cells preenchendo com cores
   - Verde = no target
   - Amarelo = slightly off
   - Vermelho = needs correction

3. Dirigir o carro 10-20 minutos:

   Cobrir todas as regiões:
   □ Idle prolongado (30s)
   □ Cruise leve (30-40% TPS)
   □ Acelerações médias (50-70% TPS)
   □ WOT pulls (100% TPS) - safe place!
   □ Várias RPM ranges (2000-6000)

   ⚠️ Evitar:
     ✗ Traffic (stop-and-go)
     ✗ Subidas longas (superaquece)
     ✗ Runs muito longos WOT (>5s)

4. Retornar ao TunerStudio

5. [VE Analyze] → [Accept Corrections]

   Mostra tabela com sugestões:
     +5% aqui
     -3% ali
     etc.

6. [Apply] → [Burn to ECU]

7. Repetir 2-3 vezes até:
   - Todas cells verdes
   - Lambda sempre no target ±0.02
```

**Tips auto-tune:**
```
✓ Fazer em várias condições (quente, frio, altitude)
✓ Combustível consistente (mesma gasolina)
✓ Não confiar 100% - verificar manual depois
✓ Ignorar cells com <5 samples (pouco dado)
✓ Desabilitar se lambda oscilar muito
```

### 13.3 Ajuste Fino Ignição (MANUAL!)

**⚠️ AUTO-TUNE NÃO AJUSTA IGNIÇÃO!**

**Procedimento SEGURO:**

```
1. COMEÇAR CONSERVADOR (valores seção 10.2)

2. Ajustar 1 cell por vez:

   Exemplo: 4000 RPM, 100 kPa (WOT)

   a. Valor inicial: 17°
   b. Test run WOT 3-5 segundos
   c. Observar:
      - Detonação? (ouvir/knock sensor)
      - EGT? (não passar 900°C)
      - Potência sente melhor?

   d. Se OK: aumentar +1° → 18°
   e. Repetir test run

   f. Continuar até:
      - Detonation começa
      - ou EGT muito alto
      - ou potência não melhora

   g. VOLTAR -3° do limite
      (margem de segurança)

3. Repetir para outras cells

⚠️ NUNCA ultrapassar:
   - WOT aspirado: 26° máx
   - WOT turbo: 18° máx (sem boost) / 12° (com boost)
```

**Ferramentas ajuste ignição:**
```
Must-have:
  ✓ Knock sensor (ou ouvido atento)
  ✓ Timing light

Recommended:
  ✓ EGT sensor (exhaust gas temp)
  ✓ Dyno (mede potência real)
  ✓ Wide-open road ou pista
```

### 13.4 Refinar Idle

**Idle instável = experiência ruim!**

```
╔══════════════════════════════════════╗
║ Idle Tuning                          ║
╠══════════════════════════════════════╣

1. Motor quente, idle 850 RPM

2. Ajustar VE @ idle:

   VE Table cell: 850 RPM, 40 kPa

   Observar Lambda:
     Target: 1.00
     Atual: ?

   Se Lambda = 1.05 (lean):
     VE +5% (ex: 75 → 79)

   Se Lambda = 0.95 (rich):
     VE -5% (ex: 75 → 71)

   Burn → Testar → Repetir

3. Ajustar Advance @ idle:

   Range: 9-13° típico

   Começar: 10°

   Aumentar +1° por vez até:
     - Idle mais suave ✓
     - ou começa instável ✗

   Ideal AP 1.8: 11-12° @ idle

4. Ajustar Idle Control:

   Se RPM oscila (820-880):
     → Reduzir P gain (2.0 → 1.5)
     → Aumentar I gain (0.5 → 0.8)

   Se RPM responde devagar:
     → Aumentar P gain (2.0 → 2.5)

5. Testar com cargas:
   □ AC ligado → idle +100 RPM OK?
   □ Headlights → idle não cai?
   □ Steering (direção hidráulica) → OK?
```

### 13.5 Refinar Aceleração

**Hesitação = enrichment insuficiente**

```
Teste:
  1. Idle estável
  2. Pisar acelerador RÁPIDO (50-100% TPS)
  3. Observar:
     - Lambda spike lean? (>1.10)
     - Hesitação/falha?
     - RPM sobe smooth?

Se hesita:
  Accel Enrichment → Aumentar +20%

  Exemplo:
    TPS 50%/s: 100% → 120%

Se afoga (fumaça preta):
  Accel Enrichment → Diminuir -20%

  Exemplo:
    TPS 50%/s: 100% → 80%

Se demora voltar (fica rico):
  Decay time: 500ms → 300ms
```

---

## 14. DATA LOGGING

### 14.1 Configurar Logging

**Menu: Tools → Data Logging → Settings**

```
╔══════════════════════════════════════╗
║ Data Logging Configuration           ║
╠══════════════════════════════════════╣

Log Rate: 10 Hz (100ms)
  (10 samples por segundo - suficiente)

Log Location:
  C:\Users\...\Documents\TunerStudioProjects\GOL_AP18\Logs\

Auto-name: YES
  (usa timestamp: 2025-01-07_14-30-00.msl)

Fields to Log:
  ✓ Time (seconds)
  ✓ RPM
  ✓ MAP (kPa)
  ✓ TPS (%)
  ✓ Lambda/AFR
  ✓ CLT (°C)
  ✓ IAT (°C)
  ✓ Battery (V)
  ✓ Advance (deg)
  ✓ VE (%)
  ✓ Pulsewidth (ms)
  ✓ Duty cycle (%)

  Turbo adicional:
  ✓ Boost (kPa)
  ✓ Knock count
  ✓ EGT (°C)
  ✓ Oil pressure
```

### 14.2 Fazer um Log

```
1. Conectar laptop no carro (inversor 12V→110V)

2. TunerStudio: [Start Logging] (botão REC)
   → Status: "Recording..."

3. Dirigir normalmente:
   - Test run WOT
   - Cruise
   - Idle
   - (qualquer condição que quer analisar)

4. [Stop Logging]
   → Arquivo .msl salvo

5. Desconectar (ou continuar monitorando)
```

### 14.3 Analisar Logs (MegaLogViewer)

**Download:** https://megalogviewer.com

```
1. Abrir MegaLogViewer

2. File → Open → selecionar .msl

3. Gráficos úteis:

   WOT Analysis:
     - RPM vs Lambda (deve estar 0.85-0.88)
     - RPM vs Advance (verificar se não detonou)
     - RPM vs VE (eficiência)

   Accel Analysis:
     - Time vs TPS (rampa aceleração)
     - Time vs Lambda (spike lean?)
     - Time vs RPM (smooth?)

   Knock Detection:
     - Knock count vs RPM
     - Knock vs Advance (momento que detonou)
     - Knock vs Boost (turbo)

4. Identificar problemas:
   - Lambda >1.10 em WOT? (danger!)
   - Knock events? (retard needed)
   - VE inconsistent? (re-tune)
```

### 14.4 SD Card Logging (SCG-ECU)

**Vantagem:** Não precisa laptop no carro!

```
Hardware:
  - SD card slot (SCG-ECU tem nativo)
  - MicroSD card (4-32GB)
  - Format: FAT32

Configuração (firmware):
  speeduino/storage.cpp:
    SD_LOGGING = true
    LOG_RATE = 10 Hz

Como usar:
  1. Inserir SD card formatado
  2. Ligar ECU → LED SD pisca
  3. Dirigir normalmente
  4. Desligar ignição
  5. Remover SD card
  6. Copiar .csv para PC
  7. Abrir no MegaLogViewer ou Excel
```

---

## 15. TROUBLESHOOTING

### 15.1 Problemas de Conexão

**TunerStudio não conecta**
```
Sintomas:
  - "No response from controller"
  - "Port busy"
  - "Timeout"

Verificar:
  □ Porta COM correta? (Device Manager)
  □ Baud rate = 115200?
  □ Cabo USB bom? (alguns não têm data)
  □ Outro programa usando porta? (Arduino IDE, etc)
  □ ECU alimentada? (12V + ignição ON)
  □ Firmware gravado? (LED pisca?)

Solução:
  1. Desconectar USB
  2. Fechar TunerStudio
  3. Reconectar USB
  4. Verificar porta (pode ter mudado COM3→COM4)
  5. Reabrir TunerStudio
  6. Test Port → deve dar OK
```

**Conecta mas perde conexão (intermitente)**
```
Causas:
  □ Cabo USB ruim
  □ Ground ruim (ECU)
  □ Interference elétrica (velas/bobinas)
  □ Voltage drop (battery fraca)

Solução:
  - Trocar cabo USB (usar com ferrite)
  - Melhorar grounds (múltiplos pontos)
  - Afastar USB de cabos de alta tensão
  - Verificar alternador (13.8-14.4V)
```

### 15.2 Problemas Idle

**Idle alto (>1000 RPM)**
```
Causas:
  □ Idle control aberto demais
  □ Vacuum leak
  □ TPS não calibrado (lê >0% em closed)
  □ Advance muito alto

Solução:
  1. Calibrar TPS (Tools → Calibrate TPS)
  2. Smoke test (procurar leaks)
  3. Reduzir Idle target temporário (850→800)
  4. Reduzir idle advance (12°→10°)
```

**Idle instável (oscila 800-900)**
```
Causas:
  □ Idle control PID mal ajustado
  □ VE table @ idle incorreta
  □ Vacuum leak intermitente
  □ Ignition coil fraca (miss)

Solução:
  1. Auto-tune VE @ idle (prolonged idle log)
  2. Ajustar PID:
     P: 2.0 → 1.5 (reduz oscilação)
     I: 0.5 → 0.8 (tracking melhor)
  3. Verificar vacuum hoses (rachaduras)
  4. Teste compressão (cilindros iguais?)
```

**Idle morto (desliga)**
```
Causas:
  □ Idle target muito baixo
  □ Idle control não funciona
  □ VE @ idle muito baixa
  □ Afterstart enrichment acaba rápido demais

Solução:
  1. Aumentar Idle target 850→950 (temporário)
  2. Hardware test → Idle valve (deve mover)
  3. Aumentar VE @ idle +10%
  4. Aumentar ASE duration 3s→5s
```

### 15.3 Problemas Aceleração

**Hesitação ao acelerar**
```
Sintomas:
  - Falha/buraco ao pisar
  - Lambda spike lean (>1.10)

Solução:
  1. Aumentar Accel Enrichment +30%
  2. Aumentar decay time 500→800ms
  3. Verificar fuel pump (pressão cai?)
  4. Verificar MAP response (slow sensor?)
```

**Fumaça preta ao acelerar**
```
Sintomas:
  - Afoga
  - Lambda very rich (<0.80)

Solução:
  1. Reduzir Accel Enrichment -30%
  2. Reduzir decay time 500→300ms
  3. Verificar fuel pressure (muito alto?)
  4. Verificar injector leak (gotejando?)
```

### 15.4 Problemas WOT

**Pouca potência (feels slow)**
```
Causas:
  □ Lambda muito lean (>0.95)
  □ Advance muito conservador (<20°)
  □ VE table muito baixa
  □ Boost leak (turbo)

Diagnóstico:
  1. Log WOT run
  2. Analisar Lambda @ WOT:
     - Deve ser 0.85-0.88
     - Se >0.90: aumentar VE +5-10%
  3. Analisar Advance @ WOT:
     - Aspirado: deve ter 24-26°
     - Se <20°: aumentar +2° (cuidado knock!)
```

**Detonação (knock)**
```
Sintomas:
  - Barulho metálico (pedregulho)
  - Perda de potência
  - EGT alto

AÇÃO IMEDIATA:
  1. PARAR de acelerar
  2. Reduzir advance -5° @ affected cells
  3. Enriquecer lambda -0.05
  4. Trocar combustível (octanagem maior)

Prevenção:
  - Sempre começar conservador
  - Monitorar knock sensor
  - EGT <900°C
  - Gasolina octanagem adequada
```

### 15.5 Problemas Sensores

**TPS errático**
```
Sintomas:
  - TPS% pulando (30→45→32)
  - Accel enrichment ativando sozinho

Solução:
  1. Aumentar TPS filter 50%→75%
  2. Verificar conexão (oxidação)
  3. Verificar voltage (deve variar suave 0.5-3.0V)
  4. Substituir TPS se desgastado
```

**MAP errático**
```
Sintomas:
  - MAP pulando ±10 kPa
  - VE cells erradas

Solução:
  1. Aumentar MAP filter 20%→50%
  2. Verificar mangueira vacuum (rachaduras)
  3. Verificar sensor voltage (analog) ou frequency
  4. Isolar sensor de vibração (rubber mount)
```

**CLT não lê corretamente**
```
Sintomas:
  - Lê -40°C: sensor desconectado
  - Lê 120°C+: sensor em curto
  - Não sobe: sensor ruim

Solução:
  1. Verificar resistência sensor:
     20°C = 3300Ω
     80°C = 300Ω
  2. Verificar fiação (corrosão)
  3. Substituir sensor
```

**Lambda não responde**
```
Sintomas:
  - Sempre lê 0.00 ou 1.00
  - Não muda com throttle

Causas:
  □ Wideband controller desligado
  □ Sensor não aqueceu (aguardar 60s)
  □ Sensor morto (idade/contaminação)
  □ Voltage calibration errada

Solução:
  1. Verificar controller power (12V)
  2. Aguardar warmup (luz fixa)
  3. Verificar calibration (0.5V=0.68, 2.5V=1.00)
  4. Substituir sensor LSU 4.9 (vida útil ~80k km)
```

---

## 16. RECURSOS DA COMUNIDADE

### 16.1 Documentação Oficial

**Speeduino Manual:**
- https://wiki.speeduino.com/
- Completo, atualizado, em inglês
- Seções essenciais:
  - Configuration
  - Tuning
  - Decoders
  - Troubleshooting

**TunerStudio Manual:**
- https://www.tunerstudio.com/
- User manual PDF
- Video tutorials

### 16.2 Fóruns

**Speeduino Forum (Internacional):**
- https://www.speeduino.com/forum/
- Muito ativo
- Threads sobre STM32
- Suporte oficial

**Speeduino Brasil:**
- Grupos Facebook:
  - "Speeduino Brasil"
  - "Injeção Programável Brasil"
- Telegram: procurar "Speeduino BR"

### 16.3 Fornecedores Brasil

**Hardware Speeduino:**
- DRS Eletric
- Kumagai Parts
- SpeedyEFI BR
- (Mercado Livre: "speeduino")

**Sensores/Componentes:**
- Injetores: Bosch, Deka
- MAP: GM 3-bar (12223861)
- Wideband: AEM, Innovate
- Roda fônica: Power Torque, WBR

### 16.4 Software

**Essenciais:**
- TunerStudio MS: https://www.tunerstudio.com/
- MegaLogViewer: https://megalogviewer.com/

**Desenvolvimento:**
- PlatformIO: https://platformio.org/
- Git: https://git-scm.com/
- VSCode: https://code.visualstudio.com/

### 16.5 Base Tunes (MSQ Files)

**Procurar por:**
- "VW AP 1.8 Speeduino tune"
- "4-cylinder distributor base tune"
- Fóruns BR: geralmente compartilham

**⚠️ NUNCA use tune de outro motor diretamente!**
- Sempre começar com base conservadora
- Auto-tune para SEU motor
- Verificar TODOS os sensores

### 16.6 Documentação SCG-ECU

**Neste repositório:**
```
docs/vw/
  - VW_GOL_AP18_COMPLETO.md (specs motor)
  - VW_GOL_QUICK_REFERENCE.md (cheat sheet)
  - VW_GOL_COMPARATIVO_VERSOES.md (hardware)
  - VW_GOL_TUNERSTUDIO_GUIA_COMPLETO.md (este arquivo)

docs/bmw/
  - (caso queira CAN integration)

reference/
  - speeduino.ini (TunerStudio interface)
```

---

## APÊNDICE A: TABELAS DE REFERÊNCIA

### A.1 Conversão Lambda ↔ AFR

```
GASOLINA (Stoich = 14.7):
Lambda    AFR      Descrição
──────────────────────────────────
0.68     10.0     Muito rico (cold start)
0.75     11.0     Rico (segurança turbo)
0.80     11.8     Rico (power)
0.85     12.5     Rico WOT (ideal aspirado)
0.90     13.2     Slightly rich
1.00     14.7     Stoichiometric (ideal)
1.05     15.4     Lean cruise (economia)
1.10     16.2     Lean (danger em WOT!)

ETANOL (Stoich = 9.0):
Lambda    AFR      Descrição
──────────────────────────────────
0.68      6.1     Muito rico
0.75      6.8     Rico (cold start)
0.80      7.2     Rico WOT (turbo)
0.85      7.7     Rico WOT (aspirado)
0.90      8.1     Slightly rich
1.00      9.0     Stoichiometric
1.05      9.5     Lean cruise
1.10      9.9     Lean
```

### A.2 Resistência NTC vs Temperatura

```
Temp (°C)   Resistance (Ω)   Voltage @ 2490Ω pullup
──────────────────────────────────────────────────
  -40         100,000            3.22V
  -20          35,000            3.08V
  -10          20,000            2.89V
    0           9,500            2.49V
   10           5,500            2.02V
   20           3,300            1.57V
   30           2,100            1.18V
   40           1,500            0.93V
   50           1,000            0.69V
   60             700            0.52V
   70             480            0.38V
   80             320            0.27V
   90             220            0.19V
  100             185            0.16V
  110             140            0.12V
  120             120            0.11V
```

### A.3 Códigos de Erro Comuns

```
Error Code   Descrição
─────────────────────────────────────────────────
0x01         CLT sensor out of range
0x02         IAT sensor out of range
0x03         O2 sensor not ready
0x04         TPS out of range
0x05         MAP sensor error
0x10         Trigger sync loss
0x11         Trigger timeout
0x20         Injector driver fault
0x21         Ignition driver fault
0x30         EEPROM write error
0x31         SD card error
```

---

## APÊNDICE B: PINOUT COMPLETO STM32F407

```
╔════════════════════════════════════════════════════╗
║  SCG-ECU STM32F407VGT6 - PINOUT COMPLETO          ║
╠════════════════════════════════════════════════════╣

PORTA (Analog Inputs):
  PA0:  TPS (ADC)           │ 0-3.3V
  PA1:  MAP (ADC/FREQ)      │ 0-3.3V ou 80-162Hz
  PA2:  CLT (ADC)           │ NTC 2490Ω
  PA3:  IAT (ADC)           │ NTC 2490Ω
  PA4:  O2 (ADC)            │ 0-3.3V (wideband)
  PA5:  VBAT (ADC)          │ 0-3.3V (divider)
  PA6:  OIL_P (ADC)         │ 0-3.3V (opcional)
  PA7:  FUEL_P (ADC)        │ 0-3.3V (opcional)
  PA8:  (reserva)
  PA9:  USART1_TX           │ Debug serial
  PA10: USART1_RX           │ Debug serial
  PA11: CAN1_RX             │ CAN bus
  PA12: CAN1_TX             │ CAN bus
  PA13: SWDIO               │ Debug (SWD)
  PA14: SWCLK               │ Debug (SWD)
  PA15: (reserva)

PORTB (Digital Inputs):
  PB0:  CRANK               │ Primary trigger
  PB1:  CAM                 │ Secondary trigger
  PB2:  KNOCK1              │ Knock sensor 1
  PB3:  KNOCK2              │ Knock sensor 2
  PB4:  CLUTCH              │ Clutch switch
  PB5:  BRAKE               │ Brake switch
  PB6:  USART1_TX (alt)
  PB7:  USART1_RX (alt)
  PB8:  CAN1_RX (alt)
  PB9:  CAN1_TX (alt)
  PB10: (I2C2_SCL)
  PB11: (I2C2_SDA)
  PB12: (SPI2_NSS)
  PB13: (SPI2_SCK)
  PB14: (SPI2_MISO)
  PB15: (SPI2_MOSI)

PORTC (Injector Outputs):
  PC0:  INJ1                │ Injector 1
  PC1:  INJ2                │ Injector 2
  PC2:  INJ3                │ Injector 3
  PC3:  INJ4                │ Injector 4
  PC4:  INJ5                │ Injector 5 (reserva)
  PC5:  INJ6                │ Injector 6 (reserva)
  PC6:  (USART6_TX)
  PC7:  (USART6_RX)
  PC8:  (SDIO_D0)
  PC9:  (SDIO_D1)
  PC10: (SDIO_D2)
  PC11: (SDIO_D3)
  PC12: (SDIO_CK)
  PC13: LED_BUILTIN         │ Status LED
  PC14: (OSC32_IN)
  PC15: (OSC32_OUT)

PORTD (Ignition Outputs):
  PD0:  IGN1                │ Coil 1
  PD1:  IGN2                │ Coil 2
  PD2:  IGN3/SDIO_CMD       │ Coil 3 (ou SD card)
  PD3:  IGN4                │ Coil 4
  PD4:  (reserva)
  PD5:  (USART2_TX)
  PD6:  (USART2_RX)
  PD7:  (reserva)
  PD8-15: (reserva)

PORTE (PWM/Aux Outputs):
  PE0:  IDLE_CTRL           │ Idle valve PWM
  PE1:  BOOST_CTRL          │ Boost solenoid
  PE2:  VVT                 │ VVT solenoid
  PE3:  FAN                 │ Fan relay
  PE4:  FUEL_PUMP           │ Fuel pump relay
  PE5:  TACHO               │ Tacho output
  PE6:  AUX1                │ Auxiliary 1
  PE7:  AUX2                │ Auxiliary 2
  PE8-15: (reserva)

USB:
  PA11: USB_DM (also CAN_RX)
  PA12: USB_DP (also CAN_TX)
  ⚠️ USB e CAN compartilham pinos - não usar juntos!

POWER:
  VDD:  3.3V (regulado)
  VBAT: 3.3V (RTC backup)
  GND:  Múltiplos pinos
  5V:   5V input (USB ou externo)
  VIN:  12V input (regulado para 3.3V)

╚════════════════════════════════════════════════════╝
```

---

## APÊNDICE C: SPEEDUINO.INI - CONFIGURAÇÕES IMPORTANTES

**Arquivo:** `reference/speeduino.ini`

```ini
[MegaTune]
   signature = "speeduino 202504-dev"

[TunerStudio]
   iniSpecVersion = 3.64

[SettingGroups]
   settingGroup = mcu, "Controller in use"
   settingOption = DEFAULT, "Arduino Mega 2560"
   settingOption = mcu_teensy, "Teensy"
   settingOption = mcu_stm32, "STM32"  ← Selecionar este!

[Constants]
   ; Engine specs
   nCylinders = scalar, U08, 0, "cyl", 1, 0, 1, 12, 0
   engineType = bits, U08, 0, [0:0], "Even fire", "Odd fire"

   ; Injector config
   injOpen = scalar, U08, 0, "ms", 0.1, 0, 0, 25.5, 1

   ; Trigger config
   triggerTeeth = scalar, U08, 0, "teeth", 1, 0, 0, 255, 0
   triggerMissingTeeth = scalar, U08, 0, "teeth", 1, 0, 0, 255, 0
```

---

## CONCLUSÃO

Este guia cobre TODO o processo de configuração do TunerStudio com Speeduino (STM32F407) para o VW Gol AP 1.8.

### ✅ Checklist Final

**Completou tudo?**
```
□ Hardware conectado (seção 3)
□ TunerStudio instalado (seção 4)
□ Primeira conexão OK (seção 5)
□ Engine constants configurados (seção 6)
□ Trigger setup correto (seção 7)
□ Sensores configurados (seção 8)
□ Sensores calibrados (seção 9)
□ Tabelas base carregadas (seção 10)
□ Proteções configuradas (seção 11)
□ Primeira partida bem-sucedida (seção 12)
□ Auto-tune VE realizado (seção 13)
□ Timing verificado (timing light)
□ Idle refinado
□ Test drive completo
□ Data logging funcional (seção 14)
```

### 🎯 Próximos Passos

1. **Dirigir 500-1000 km** com tune base
2. **Refinar** tabelas progressivamente
3. **Dyno tuning** (recomendado para WOT)
4. **Trocar combustível** e re-tune se necessário
5. **Backup .msq** regularmente!

### ⚠️ LEMBRAR SEMPRE

```
"Rich is safe, lean is mean, knock is GAME OVER"

✓ Começar conservador
✓ Ajustar progressivamente
✓ Monitorar lambda SEMPRE
✓ Backup antes de mudar
✓ Documentar as mudanças
```

### 📚 Documentação Relacionada

- `VW_GOL_AP18_COMPLETO.md` - Specs completas motor
- `VW_GOL_QUICK_REFERENCE.md` - Cheat sheet valores
- `VW_GOL_COMPARATIVO_VERSOES.md` - Hardware options
- `02_PROTOCOLO_CAN_BMW_E46.md` - CAN integration (se aplicável)

---

**Versão:** 1.0
**Última atualização:** 07/01/2025
**Autor:** SCG-ECU Development Team
**Licença:** Open Source (compatível com Speeduino GPL)

```
═══════════════════════════════════════════════════════════════
           BOA SORTE COM SEU PROJETO!

    "The best tune is the one you can drive home with"
                 - Tuner Wisdom
═══════════════════════════════════════════════════════════════
```
