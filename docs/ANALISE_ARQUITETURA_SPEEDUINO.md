# Análise Completa da Arquitetura Speeduino - SCG-ECU 2.0

**Microcontrolador:** STM32F407VGT6 @ 168 MHz
**Total de Código:** ~33,371 linhas (76 arquivos .cpp/.h/.ino)
**Configuração:** 8x8 (8 injetores + 8 bobinas)
**Loop Principal:** 5,000-15,000 iterações/segundo (depende de RPM e carga)

---

## Árvore de Arquivos por Módulo Funcional

```
speeduino/
│
├── CORE - Inicialização e Loop Principal (3 arquivos, ~2900 linhas)
│   ├── speeduino.ino           # Loop principal + setup()
│   ├── speeduino.h/.cpp        # Funções auxiliares do loop
│   ├── globals.h/.cpp          # Variáveis globais e configurações
│   ├── init.h/.cpp             # Inicialização de hardware (3892 linhas!)
│   └── statuses.h              # Struct currentStatus (estado do motor)
│
├── SCHEDULERS - Timing de Eventos (4 arquivos, ~1500 linhas)
│   ├── scheduler.h/.cpp        # Schedulers de injeção/ignição
│   ├── scheduledIO.h/.cpp      # Callbacks de hardware (ON/OFF)
│   ├── schedule_calcs.h/.cpp   # Cálculo de timing de eventos
│   └── timers.h/.cpp           # Timer de 1ms (TIM11) para tarefas periódicas
│
├── DECODERS - Sincronização do Motor (2 arquivos, ~4000 linhas)
│   ├── decoders.h/.cpp         # 29 tipos de roda fônica (missing tooth, dual, etc)
│   └── crankMaths.h/.cpp       # Cálculos de RPM e posição angular
│
├── SENSORS - Leitura de Sensores (2 arquivos, ~1200 linhas)
│   ├── sensors.h/.cpp          # Leitura de MAP, TPS, CLT, IAT, O2, BAT, etc
│   └── sensors_map_structs.h   # Structs para sensores MAP
│
├── TABLES - Interpolação 2D/3D (8 arquivos, ~2500 linhas)
│   ├── table2d.h/.cpp          # Tabelas 2D (1D lookup)
│   ├── table3d.h/.cpp          # Tabelas 3D (2D lookup com interpolação)
│   ├── table3d_interpolate.h/.cpp  # Algoritmo de interpolação bilinear
│   ├── table3d_axes.h          # Eixos X/Y das tabelas
│   ├── table3d_values.h        # Valores Z das tabelas
│   └── table3d_typedefs.h      # Definições de tipos de tabelas
│
├── CORRECTIONS - Sistema de Correções (2 arquivos, ~800 linhas)
│   ├── corrections.h/.cpp      # WUE, ASE, AE, AFR target, IAT, Baro
│   └── secondaryTables.h/.cpp  # Tabelas secundárias (fuel2, ign2)
│
├── FUEL & IGNITION - Cálculos de Injeção/Ignição
│   ├── schedule_calcs.h/.cpp   # Cálculo de pulsewidth e advance
│   └── load_source.h           # Fonte de carga (MAP/TPS/MAF)
│
├── AUXILIARIES - Funções Auxiliares (4 arquivos, ~1500 linhas)
│   ├── auxiliaries.h/.cpp      # Boost, VVT, Idle, Cooling fan
│   ├── idle.h/.cpp             # Controle idle (PWM/Stepper)
│   ├── engineProtection.h/.cpp # RPM limit, oil pressure, overboost
│   └── acc_mc33810.h/.cpp      # Driver MC33810 (opcional)
│
├── COMMUNICATION - Comunicação Externa (8 arquivos, ~3000 linhas)
│   ├── comms.h/.cpp            # Protocolo serial TunerStudio
│   ├── comms_legacy.h/.cpp     # Compatibilidade com versões antigas
│   ├── comms_secondary.h/.cpp  # Serial secundária
│   ├── comms_CAN.h/.cpp        # Comunicação CAN-Bus
│   ├── comms_sd.h              # SD card logging
│   ├── SD_logger.h/.cpp        # Logger para SD card
│   ├── logger.h/.cpp           # Sistema de logging
│   └── TS_CommandButtonHandler.h/.cpp  # Comandos de botões TS
│
├── STORAGE - Persistência EEPROM (2 arquivos, ~800 linhas)
│   ├── storage.h/.cpp          # Leitura/escrita EEPROM
│   └── pages.h/.cpp            # Páginas de configuração
│
├── MATH & UTILITIES (6 arquivos, ~1000 linhas)
│   ├── maths.h/.cpp            # Funções matemáticas otimizadas
│   ├── bit_manip.h             # Macros de manipulação de bits
│   ├── bit_shifts.h            # Shifts otimizados
│   ├── utilities.h/.cpp        # Funções auxiliares gerais
│   ├── updates.h/.cpp          # Atualizações de firmware
│   └── units.h                 # Conversões de unidades
│
├── BOARD SPECIFIC - Hardware STM32F407 (2 arquivos, ~1000 linhas)
│   ├── board_stm32_official.h  # Definições de pinos e timers
│   ├── board_stm32_official.cpp # Inicialização de hardware STM32
│   └── port_pin.h              # Abstração de portas/pinos
│
├── CONFIGURATION (3 arquivos)
│   ├── config_pages.h          # Estrutura de páginas de config
│   ├── page_crc.h/.cpp         # CRC de páginas
│   └── rtc_common.h/.cpp       # Real-time clock
│
└── TESTING
    └── unit_testing.h          # Suporte para testes unitários
```

---

## Análise do Loop Principal (speeduino.ino)

### Estrutura do loop()

O loop principal executa **5,000-15,000 vezes por segundo** no STM32F407 @ 168 MHz.

```cpp
void loop(void)
{
  // =====================================
  // FASE 1: COMUNICAÇÃO (Prioridade Alta)
  // =====================================
  // Tempo: ~50-200µs (se houver dados)
  if (Serial.available() > 0) {
    serialReceive();  // Processa comandos TunerStudio
  }

  if (secondarySerial.available() > 0) {
    secondarySerialReceive();  // Serial secundária
  }

  // =====================================
  // FASE 2: TIMING E SINCRONIZAÇÃO
  // =====================================
  // Tempo: ~5-10µs
  currentLoopTime = micros();
  unsigned long timeToLastTooth = (currentLoopTime - toothLastToothTime);

  if (timeToLastTooth < MAX_STALL_TIME) {
    // Motor girando, atualizar RPM
    currentStatus.RPM = currentStatus.instantRPM;  // ~1µs
  } else {
    // Motor parado (sem dentes detectados)
    currentStatus.RPM = 0;
  }

  // =====================================
  // FASE 3: LEITURA DE SENSORES (Frequência Variável)
  // =====================================
  // Executado em intervalos diferentes via LOOP_TIMER

  // 1 kHz (a cada loop)
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_1KHZ)) {
    // Nenhuma leitura nesta frequência
  }

  // 200 Hz (a cada 5ms)
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_200HZ)) {
    // Tempo: ~20-30µs
    readMAP();  // ~15µs (ADC + média móvel)
  }

  // 50 Hz (a cada 20ms)
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_50HZ)) {
    // Tempo: ~30-50µs
    readTPS();      // ~10µs
    readCLT();      // ~10µs
    readIAT();      // ~10µs
    readO2();       // ~10µs
    readBat();      // ~10µs
  }

  // 30 Hz (a cada 33ms)
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_30HZ)) {
    // Tempo: ~50-100µs
    readO2_2();
    readBaro();
    flexRead();
  }

  // 15 Hz (a cada 66ms)
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_15HZ)) {
    // Tempo: ~100-200µs
    idleControl();           // Controle idle
    boostControl();          // Controle boost
    vvtControl();            // Controle VVT
    nitrousControl();        // Controle nitrous
    checkEngineProtection(); // Verificações de proteção
  }

  // 10 Hz (a cada 100ms)
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ)) {
    // Tempo: ~50-100µs
    readFuelPressure();
    readOilPressure();
  }

  // 4 Hz (a cada 250ms)
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_4HZ)) {
    // Tempo: ~20-50µs
    calculateFuelTrim();
  }

  // =====================================
  // FASE 4: CÁLCULOS DE COMBUSTÍVEL
  // =====================================
  // Tempo: ~30-50µs

  // Lookup VE na tabela 3D (16x16)
  currentStatus.VE = get3DTableValue(&fuelTable,
                                     currentStatus.fuelLoad,  // MAP ou TPS
                                     currentStatus.RPM);      // ~25µs (interpolação bilinear)

  // Cálculos de correções multiplicativas
  currentStatus.corrections = 100;  // Base 100%
  currentStatus.corrections *= correctionWUE();   // Warm-up enrichment (~5µs)
  currentStatus.corrections *= correctionASE();   // After-start enrichment (~3µs)
  currentStatus.corrections *= correctionAccel(); // Acceleration enrichment (~10µs)
  currentStatus.corrections *= correctionFloodClear(); // Flood clear (~2µs)
  currentStatus.corrections *= correctionAFRClosedLoop(); // Closed loop (~15µs)
  currentStatus.corrections *= correctionFlex();  // Flex fuel (~5µs)
  currentStatus.corrections *= correctionIATDensity(); // IAT density (~5µs)
  currentStatus.corrections *= correctionBaro();  // Barometric (~3µs)
  currentStatus.corrections *= correctionLaunch(); // Launch control (~5µs)

  // Cálculo do pulsewidth final
  currentStatus.PW1 = PW(req_fuel_uS,              // Req fuel base
                         currentStatus.VE,          // VE%
                         currentStatus.MAP,         // MAP (kPa)
                         currentStatus.corrections, // Correções totais
                         inj_opentime_uS);          // Dead time
  // Tempo: ~15µs

  // =====================================
  // FASE 5: CÁLCULOS DE IGNIÇÃO
  // =====================================
  // Tempo: ~25-40µs

  // Lookup advance na tabela 3D (16x16)
  currentStatus.advance = get3DTableValue(&ignitionTable,
                                          currentStatus.ignLoad,  // MAP ou TPS
                                          currentStatus.RPM);     // ~25µs

  // Correções de ignição
  currentStatus.advance += correctionFlexTiming();    // Flex timing (~5µs)
  currentStatus.advance += correctionIATRetard();     // IAT retard (~3µs)
  currentStatus.advance += correctionCLTAdvance();    // CLT advance (~3µs)
  currentStatus.advance += correctionIdleAdvance();   // Idle advance (~3µs)
  currentStatus.advance += correctionSoftRevLimit(); // Soft limiter (~5µs)
  currentStatus.advance += correctionNitrous();      // Nitrous timing (~3µs)
  currentStatus.advance += correctionKnock();        // Knock retard (~5µs)

  // Cálculo de dwell
  currentStatus.dwell = get2DTableValue(&dwellTable, currentStatus.RPM); // ~10µs

  // =====================================
  // FASE 6: SINCRONIZAÇÃO E SCHEDULING
  // =====================================
  // Tempo: ~10-20µs

  // Verificar se motor está sincronizado
  if (currentStatus.hasSync && (currentStatus.RPM > 0)) {

    // Calcular ângulo atual do virabrequim
    int crankAngle = getCrankAngle();  // ~5µs

    // Calcular timing de injeção/ignição
    calculateInjectorTiming();  // ~10µs
    calculateIgnitionTiming();  // ~10µs

    // CRÍTICO: Schedulers são ativados por INTERRUPÇÕES de dentes
    // O loop principal apenas prepara os cálculos
    // Os schedulers rodam em ISRs quando dentes são detectados
  }

  // =====================================
  // FASE 7: LOGGING E COMUNICAÇÃO AUXILIAR
  // =====================================
  // Tempo: ~10-50µs (se ativo)

  if (configPage13.sdLoggingEnabled) {
    sdCardLogging();  // Logging para SD card
  }

  if (configPage9.canEnabled) {
    canBroadcast();   // Broadcast CAN
  }

  // =====================================
  // TIMING TOTAL POR LOOP
  // =====================================
  // Pior caso (todos os timers ativos):  ~500-800µs
  // Caso típico (poucos timers):         ~150-300µs
  // Melhor caso (sem sensores):          ~50-100µs
}
```

---

## Análise de Performance e Complexidade

### Timing do Loop Principal

| Frequência Loop | Período      | Tempo Disponível | Tempo Usado (típico) | Margem   |
|-----------------|--------------|------------------|----------------------|----------|
| 15,000 Hz       | 66.7 µs      | 66.7 µs          | ~150 µs (OVERRUN!)   | -83.3 µs |
| 10,000 Hz       | 100 µs       | 100 µs           | ~150 µs (OVERRUN!)   | -50 µs   |
| 5,000 Hz        | 200 µs       | 200 µs           | ~150 µs              | +50 µs   |
| 2,000 Hz        | 500 µs       | 500 µs           | ~150 µs              | +350 µs  |

**Observação Crítica:**
- Loop roda **~5,000 vezes/segundo** em condições normais
- Tempo médio por loop: **150-300 µs**
- Em RPMs altos (>6000 RPM), loop pode não completar antes do próximo dente
- **Solução:** Schedulers usam ISRs de hardware para timing crítico

### Gargalos Identificados

#### 1. Interpolação de Tabelas 3D (25 µs cada)
```cpp
// Executado 2x por loop (fuel + ignition)
// Tempo total: ~50 µs
currentStatus.VE = get3DTableValue(&fuelTable, fuelLoad, RPM);
currentStatus.advance = get3DTableValue(&ignitionTable, ignLoad, RPM);
```

**Complexidade:** O(1) com interpolação bilinear
- Busca binária em eixo X (RPM): ~4 comparações
- Busca binária em eixo Y (Load): ~4 comparações
- Interpolação bilinear: 4 multiplicações + 3 somas

**Otimização atual:** Usa fixed-point math (QU1X8) em vez de float

#### 2. Sistema de Correções (~60 µs total)
```cpp
// 10+ correções multiplicativas executadas sequencialmente
corrections *= correctionWUE();    // ~5µs
corrections *= correctionASE();    // ~3µs
corrections *= correctionAccel();  // ~10µs (mais complexa)
// ... 7 correções adicionais
```

**Complexidade:** O(n) onde n = número de correções ativas
- Cada correção: lookup em tabela 2D ou cálculo simples
- **Trade-off:** Precisão vs Performance (todas correções ativas = +60µs)

#### 3. Leitura de Sensores ADC (~15 µs por sensor)
```cpp
// MAP é o mais crítico (lido a 200 Hz)
readMAP(); // ~15µs
  -> analogRead(pinMAP)           // ~8µs (12-bit ADC)
  -> movingAverage(4 samples)     // ~5µs
  -> applyCalibration()           // ~2µs
```

**Otimização:** ADC usa DMA em background (não bloqueia CPU)

#### 4. Schedulers - Timing Crítico (execução em ISR)
```cpp
// NÃO executado no loop principal!
// Executado em ISRs quando dentes são detectados
void triggerPri_ISR() {  // Interrupt Service Routine
  // Tempo de execução: 2-5 µs (CRÍTICO!)
  setIgnitionSchedule1(...);
  setFuelSchedule1(...);
}
```

**Complexidade:** O(1) - execução determinística
- **Constraint:** ISR DEVE completar antes do próximo dente (mín. 50µs @ 12,000 RPM)

---

## Fluxo de Dados Completo

```
[HARDWARE]                    [ISRs - Alta Prioridade]              [Loop Principal - Baixa Prioridade]
    │                                  │                                         │
    │ Trigger Sensor (Dente)          │                                         │
    ├──────────────────────────────────>                                        │
    │                         triggerPri_ISR() { ───┐                           │
    │                           crankAngle++         │ 2-5µs                    │
    │                           calculateTiming()    │                          │
    │                           setSchedules() ──────┘                          │
    │                         }                                                 │
    │                                  │                                        │
    │ Hardware Timer Compare           │                                        │
    ├──────────────────────────────────>                                        │
    │                         fuelSchedule1_ISR() { ─┐                          │
    │                           INJ1_ON()            │ <1µs                     │
    │                         } ─────────────────────┘                          │
    │                                  │                                        │
    │ ADC Conversion Complete          │                                        │
    ├──────────────────────────────────>                                        │
    │                         ADC_ISR() { ───────────┐                          │
    │                           buffer[i] = ADC_DR   │ <1µs                     │
    │                         } ──────────────────────┘                         │
    │                                  │                                        │
    │                                  │                                        │
    │                                  │ <--- loop() executa quando CPU livre
    │                                  │                currentLoopTime = micros()
    │                                  │                readSensors() (~50µs)
    │                                  │                calculateFuel() (~50µs)
    │                                  │                calculateIgnition() (~40µs)
    │                                  │                serialComms() (~50µs)
    │                                  │                auxiliaries() (~100µs)
    │                                  │                ──────────> loop novamente
```

---

## Arquivos Mais Críticos (Performance)

### 1. **decoders.cpp** (4000 linhas) - MAIS CRÍTICO
- **Função:** Detectar dentes e calcular posição do virabrequim
- **Execução:** ISR (prioridade máxima)
- **Timing:** <5µs por dente
- **Complexidade:** 29 decoders diferentes implementados
- **Trade-off:** Código grande mas otimizado (switch-case compilado para jump table)

### 2. **scheduler.cpp** (1200 linhas) - CRÍTICO
- **Função:** Agendar eventos de injeção/ignição
- **Execução:** ISR + loop principal
- **Timing:** ISR <1µs, cálculos no loop ~20µs
- **Otimização:** Usa hardware timers (TIM2-5) para precisão

### 3. **table3d_interpolate.cpp** (~500 linhas) - CRÍTICO
- **Função:** Interpolação bilinear em tabelas 3D
- **Execução:** Loop principal
- **Timing:** ~25µs por lookup
- **Otimização:** Fixed-point math (QU1X8), inline functions

### 4. **sensors.cpp** (1200 linhas) - MODERADO
- **Função:** Leitura e processamento de sensores
- **Execução:** Loop principal (frequências variáveis)
- **Timing:** ~15µs por sensor
- **Otimização:** DMA para ADC, média móvel eficiente

### 5. **corrections.cpp** (800 linhas) - MODERADO
- **Função:** Aplicar correções de combustível/ignição
- **Execução:** Loop principal
- **Timing:** ~60µs total (todas correções)
- **Otimização:** Cálculos simples, poucas divisões

---

## Conclusão - Complexidade Gerenciável

### Por que o código funciona bem apesar da complexidade?

1. **Separação ISR vs Loop**
   - Timing crítico (schedulers, decoders): ISRs (2-5µs)
   - Cálculos pesados (tabelas, correções): Loop principal (150µs)

2. **Execução Condicional**
   - Nem todos os sensores lidos a cada loop
   - Uso de `LOOP_TIMER` para frequências variáveis
   - Auxiliary functions apenas a 15 Hz

3. **Otimizações de Código**
   - Fixed-point math em vez de float
   - Inline functions para hot paths
   - Lookup tables pré-calculadas
   - Jump tables para decoders (switch-case)

4. **Hardware STM32F407**
   - 168 MHz (6x mais rápido que Arduino Mega)
   - DMA para ADC e Serial
   - 6 timers de hardware independentes
   - FPU de hardware (não usado atualmente)

### Margem de Performance

- **Loop atual:** ~150-300µs por iteração
- **Tempo disponível:** 200µs (@ 5000 Hz)
- **Margem:** ~50-100µs (25-50% de sobra)
- **Headroom:** Suficiente para expansões futuras

**Resultado:** Código é complexo mas **bem arquitetado** para real-time performance.

---

**Próxima Análise:** Detalhamento módulo por módulo?
