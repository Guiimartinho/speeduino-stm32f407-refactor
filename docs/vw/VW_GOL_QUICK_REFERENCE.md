# VW GOL AP 1.8 - QUICK REFERENCE GUIDE
## GUIA RÁPIDO DE CONSULTA - SCG-ECU 2.0

**Versão:** 1.0
**Data:** 01/11/2025
**Tipo:** Referência Rápida (Cheat Sheet)

---

## ESPECIFICAÇÕES MOTOR AP 1.8

```
Cilindros:          4 em linha
Cilindrada:         1781 cm³
Bore x Stroke:      81.0 x 86.4 mm
Compressão:         10.0:1 (MI gasolina)
Ordem ignição:      1-3-4-2
Cilindro #1:        Lado polia
Potência:           97 cv @ 5,250 RPM
Torque:             15.5 kgfm @ 3,000 RPM
Combustível:        Gasolina ou Etanol
```

---

## PINOUT RÁPIDO SCG-ECU

### Entradas Analógicas
```
PA0: TPS (0-5V)
PA1: MAP (0-5V ou freq)
PA2: CLT (NTC)
PA3: IAT (NTC)
PA4: O2/Wideband (0-5V)
PA5: Battery Voltage
PA6: Oil Pressure (turbo)
PA7: Fuel Pressure
```

### Entradas Digitais
```
PB0: Crank (Hall/VR)
PB1: Cam (sequential)
PB2: Knock 1
PB3: Knock 2
PB4: Clutch Switch
PB5: Brake Switch
```

### Saídas Injeção
```
PC0-PC3: Injetores 1-4
PC4-PC5: Injetores 5-6 (reserva)
```

### Saídas Ignição
```
PD0-PD3: Bobinas 1-4
```

### Saídas PWM
```
PE0: Idle Control
PE1: Boost Control
PE2: VVT
PE3: Fan
PE4: Fuel Pump
```

---

## SENSORES CALIBRAÇÃO

### TPS
```
Tipo:       Potenciômetro 3 fios
Range:      0-5V
Closed:     ~0.5V (10%)
WOT:        ~4.5V (90%)
```

### MAP
```
Original:   Frequency (80-162 Hz)
Upgrade:    GM 3-bar (0.5-4.5V)
  0.5V = 0 kPa
  2.5V = 100 kPa (atm)
  4.5V = 300 kPa (2 bar boost)
```

### CLT/IAT (NTC)
```
Pullup:     2490 Ω
-40°C:      100kΩ
0°C:        6kΩ
25°C:       2.5kΩ
80°C:       300Ω
100°C:      180Ω
```

### Lambda/O2
```
Narrowband: 0.1-0.9V (NÃO usar!)
Wideband:   LSU 4.9 (obrigatório)
  0.5V = Lambda 0.68
  2.5V = Lambda 1.00
  4.5V = Lambda 1.36
```

---

## CONFIGURAÇÃO RÁPIDA

### Decoder (Distribuidor Hall)
```
Type:       BASIC_DISTRIBUTOR
Trigger:    HALL
Edge:       RISING
Cyl/rev:    2
Teeth:      4
```

### Decoder (Roda Fônica 60-2)
```
Type:       MISSING_TOOTH
Teeth:      60
Missing:    2
Edge:       RISING
Angle:      90° BTDC (ajustar)
```

### Injeção Aspirado
```
Mode:       SEQUENTIAL ou SEMI_SEQUENTIAL
Injectors:  4
Flow:       280 cc/min @ 3 bar
Pressure:   3.0 bar
Dead time:  1.0 ms @ 14V
```

### Injeção Turbo
```
Mode:       SEQUENTIAL
Injectors:  4
Flow:       440 cc/min @ 3 bar
Pressure:   3.0 bar + boost ref
Dead time:  0.95 ms @ 14V
```

### Ignição Aspirado
```
Mode:       WASTED_SPARK_DISTRIBUTOR
Outputs:    1
Dwell:      3.0 ms @ 14V
```

### Ignição Turbo
```
Mode:       SEQUENTIAL
Outputs:    4
Coils:      Coil-on-plug
Dwell:      4.0 ms @ 14V
```

### Ar Condicionado (Opcional)
```
Pin controle:   PE3 (relay AC)
Pin sensor:     PA7 (pressão AC 0-300 psi)
Idle-up:        +100 RPM
Idle duty:      +8% (motor valve)
WOT disable:    TPS >90% (corta AC)
CLT disable:    CLT >105°C (proteção)
Min RPM:        650 RPM (permite ligar)
```

### Câmbio Sequencial (Opcional)
```
Paddle UP:      PB4 (DIGITAL, ACTIVE_LOW)
Paddle DOWN:    PB5 (DIGITAL, ACTIVE_LOW)
Solenoid UP:    PE6 (PWM 100Hz)
Solenoid DOWN:  PE7 (PWM 100Hz)
Pulse width:    60 ms
Duty cycle:     80%
Debounce:       20 ms

Flat shift:
  Spark cut:    100% (corta ignição)
  Fuel cut:     0% (MANTER - crítico turbo!)
  Cut time:     120 ms
  Min RPM:      3000 RPM
  Min TPS:      50%
```

---

## VALORES BASE TUNING

### RPM Limits
```
ASPIRADO:
  Idle:         850 RPM
  Soft limit:   6,000 RPM
  Hard limit:   6,500 RPM

TURBO:
  Idle:         850 RPM
  Soft limit:   6,500 RPM
  Hard limit:   7,000 RPM
```

### Ignition Advance (Gasolina)
```
ASPIRADO:
  Idle:     9-12°
  Cruise:   18-22°
  WOT:      24-26°

TURBO:
  Idle:     12°
  Cruise:   18-22° (sem boost)
  WOT:      12-16° (com boost)

  Boost retard:
    0.5 bar: -6°
    1.0 bar: -12°
```

### AFR Targets (Gasolina)
```
ASPIRADO (Lambda):
  Idle:     1.00 (14.7:1)
  Cruise:   1.05 (15.4:1)
  WOT:      0.85 (12.5:1)

TURBO (Lambda):
  Idle:     1.00 (14.7:1)
  Cruise:   1.05 (15.4:1) - sem boost
  WOT:      0.82 (12.0:1) - com boost
```

### AFR Targets (Etanol)
```
ASPIRADO:
  Idle:     1.00 (9.0:1)
  Cruise:   1.05 (9.5:1)
  WOT:      0.85 (7.7:1)

TURBO:
  Idle:     1.00 (9.0:1)
  Cruise:   1.05 (9.5:1)
  WOT:      0.80 (7.2:1)
```

### VE Table Base (Aspirado)
```
Idle (850 RPM):     70-75%
Cruise (2000):      80-85%
WOT (4000):         90-92%
Peak (5000):        92-93%
```

### VE Table Base (Turbo)
```
Idle:           75%
Cruise (NA):    85%
Boost 0.5 bar:  95-100%
Boost 1.0 bar:  105-112%
```

---

## PROTEÇÕES CONFIGURAÇÃO

### Aspirado
```
CLT Warning:    105°C
CLT Limp:       110°C
DFCO Activate:  1800 RPM
DFCO Deactive:  1500 RPM
```

### Turbo
```
CLT Warning:    100°C
CLT Limp:       105°C
Boost Cut:      108°C
Max Boost:      1.1 bar (hard limit)
Oil Min Idle:   1.0 bar
Oil Min Cruise: 3.0 bar
EGT Max:        900°C
EGT Warning:    850°C
```

---

## CORREÇÕES FUEL

### WUE (Warmup Enrichment)
```
Temp (°C)   Enrich (%)
---------   ----------
   -20         200
     0         150
    20         125
    40         110
    60         102
    80         100
```

### ASE (Afterstart)
```
Time (s)    Enrich (%)
--------    ----------
    0           150
    1           130
    3           105
    5           100
```

### Accel Enrichment
```
TPS Rate    Amount
(/s)        (%)
--------    ------
   10         25
   50        100
  100        150
```

---

## BOOST CONTROL (TURBO)

### Targets Base
```
RPM\TPS     0%      50%     100%
-------------------------------
2000       100     110     140  (kPa)
3000       100     120     160
4000       100     130     180
5000       100     140     200
6000       100     140     200
```

### PID Gains
```
kP:         2.5
kI:         0.8
kD:         0.1
Slew rate:  10 kPa/s
```

---

## KNOCK CONTROL (TURBO)

```
Threshold:      2.5V
Frequency:      5-15 kHz
Peak freq:      7500 Hz (AP 1.8)
Window:         40° após TDC
Retard/event:   -3°
Max retard:     -12°
Recovery:       0.5°/s
```

---

## DWELL TABLE

```
Voltage (V)   Dwell (ms)
-----------   ----------
   10.0          4.5
   12.0          3.5
   14.0          3.0
   16.0          2.8
```

---

## COMPONENTES BOM RÁPIDO

### Aspirado (~R$ 3,000)
```
4x Injetores 280cc       R$ 400
Flauta combustível       R$ 150
Regulador 3 bar          R$ 200
Bomba 255 L/h            R$ 350
Wideband LSU 4.9         R$ 1000
MAP 3-bar GM             R$ 120
Roda fônica 60-2 (opc)   R$ 250
Chicote + conectores     R$ 450
```

### Turbo Adicional (~R$ 12,880)
```
Turbo GT2860RS           R$ 3500
Wastegate 38mm           R$ 800
Intercooler FMIC         R$ 1200
Downpipe + coletor       R$ 1500
Injetores 440cc          R$ 600
4x Coil packs            R$ 600
Sensores (knock/oil/egt) R$ 1180
Tubulação + BOV          R$ 1200
Junta MLS + ARP          R$ 1200
Misc hardware            R$ 1100
```

### Ar Condicionado (Opcional)
```
ASPIRADO (+R$ 3,645):
  Compressor Delphi      R$ 850
  Condensador + evap     R$ 830
  Sensor + relé          R$ 195
  Tubulação + conexões   R$ 690
  Gás + instalação       R$ 515
  Fan elétrica 12"       R$ 280

TURBO (+R$ 6,075):
  Mesmo acima            R$ 3,645
  + Intercooler 600x300  R$ 1,800
  + Tubulação nova       R$ 350
  + Fan adicional        R$ 280
```

### Câmbio Sequencial (Opcional)
```
DIY (+R$ 4,930):
  Atuador pneumático     R$ 1,800
  Válvula solenóide      R$ 650
  Tanque + compressor    R$ 900
  Paddle switches        R$ 280
  Brackets + hardware    R$ 750
  Calibração             R$ 500

INSTALADO (+R$ 6,630):
  Mesmo acima            R$ 4,930
  + Mão de obra          R$ 1,200
  + Testes               R$ 500
```

---

## TROUBLESHOOTING RÁPIDO

### Não dá partida
```
□ Verificar sincronismo (timing light)
□ TPS calibrado?
□ MAP lendo corretamente?
□ Injetores ativando? (LED test)
□ Bobina ativando? (centelha)
□ Fuel pump ligando?
```

### Idle instável
```
□ Ajustar idle advance (9-12°)
□ Ajustar VE table @ idle
□ Idle control funcionando?
□ Vacuum leaks?
□ Lambda @ idle = 1.0?
```

### Hesitação aceleração
```
□ Accel enrichment muito baixo
□ TPS calibrado corretamente?
□ AE decay time (500-1000ms)
□ MAP respondendo rápido?
```

### Detonação (knock)
```
□ Reduzir advance -3-5°
□ Enriquecer AFR (lambda -0.05)
□ Gasolina octanagem OK?
□ EGT muito alto? (turbo)
□ Boost spike? (turbo)
```

### Consumo alto
```
□ Lambda muito rich?
□ VE table muito alta?
□ Accel enrichment excessivo?
□ Injector dead time errado?
□ Fuel leak?
```

### Turbo - Sem boost
```
□ Wastegate travada aberta?
□ Boost leak (smoke test)
□ Solenoid boost OK?
□ Boost target configurado?
□ Duty cycle solenoid 0%?
```

### Turbo - Overboost
```
□ Wastegate travada fechada?
□ Solenoid boost falhou?
□ Target boost muito alto?
□ Duty cycle 100%?
□ ⚠️ CORTAR BOOST IMEDIATAMENTE!
```

---

## COMANDOS TUNERSTUDIO

### Conectar ECU
```
1. Conectar USB
2. TunerStudio → Communications
3. Select COM port
4. Baud: 115200
5. Test Port → OK
6. Connect
```

### Carregar Config
```
File → Open Project
Selecionar: vw_gol_ap18_xxx.msq
File → Download → To Controller
```

### Calibrar TPS
```
Tools → Calibrate TPS
1. Throttle CLOSED → Set Closed
2. Throttle WOT → Set Open
3. Apply
```

### Ajustar VE Table
```
Tuning → VE Table
Mode: Auto-tune (com wideband)
Target: Lambda 1.0 (cruise)
Drive: 10-15 min
Analyze → Apply
```

### Data Logging
```
Tools → Data Logging
Start New Log
Drive car
Stop Log
Analyze → MegaLogViewer
```

---

## SAFETY CHECKS PRÉ-PARTIDA

### Elétrica
```
□ Battery voltage OK (12-14V)?
□ All grounds tight?
□ No shorts (multimeter)?
□ Fuses OK?
□ Sensors connected?
```

### Mecânica
```
□ Timing belt aligned?
□ Oil level OK?
□ Coolant level OK?
□ No fuel leaks?
□ No vacuum leaks?
□ Turbo oil lines OK? (turbo)
```

### ECU
```
□ Config loaded?
□ TPS calibrated?
□ MAP reading ~100 kPa?
□ CLT reading room temp?
□ Injector test OK?
□ Ignition test OK?
```

### Tuning
```
□ VE table loaded (conservative)?
□ Advance table loaded (safe)?
□ AFR target = RICH (0.85-0.90)?
□ Rev limit set?
□ Protections enabled?
```

---

## PRIMEIRA PARTIDA CHECKLIST

```
ANTES:
□ Fuel pump prime (3 sec)
□ TPS = 0% (closed throttle)
□ MAP = ~100 kPa
□ CLT = ambiente
□ Lambda controller aquecido (60s)

DURANTE:
□ Cranking = lambda RICH (0.85)
□ Cranking RPM = 200-400
□ Fuel pump ligando?
□ Centelha verificada?

APÓS PEGAR:
□ Idle RPM = 850 ± 100
□ Lambda = 1.0 ± 0.05
□ CLT subindo gradualmente
□ Oil pressure >1 bar
□ Sem fuel leaks
□ Sem vacuum leaks
□ Advance = 9-12° (verificar timing light)

SE NÃO PEGAR:
□ Verificar sync (crank sensor)
□ Aumentar enriquecimento
□ Verificar centelha
□ Verificar pressão fuel
```

---

## VALORES CRÍTICOS MONITORAR

### SEMPRE Monitorar
```
✓ RPM
✓ Lambda/AFR
✓ CLT (temperatura motor)
✓ TPS
✓ MAP
```

### TURBO Adicional
```
✓ Boost pressure
✓ Knock events
✓ EGT (exhaust temp)
✓ Oil pressure
✓ Ignition advance
```

### Limites Críticos
```
⚠️ Lambda <0.75 = MUITO RICO (limpar velas)
⚠️ Lambda >1.10 = MUITO POBRE (danger!)
⚠️ CLT >105°C = OVERHEAT
⚠️ Boost >1.1 bar = OVERBOOST (turbo)
⚠️ Knock contínuo = DANGER (turbo)
⚠️ EGT >900°C = DANGER (turbo)
⚠️ Oil <1 bar idle = DANGER (turbo)
```

---

## TUNERSTUDIO GAUGES RECOMENDADOS

### Dashboard Principal
```
┌─────────────────────────────────────┐
│  RPM: 2500    MAP: 45 kPa           │
│  Lambda: 1.02  CLT: 85°C            │
│  TPS: 25%     Advance: 20°          │
│  VE: 82%      Dwell: 3.0ms          │
└─────────────────────────────────────┘
```

### Dashboard Turbo
```
┌─────────────────────────────────────┐
│  RPM: 4500    Boost: 0.8 bar        │
│  Lambda: 0.85  EGT: 820°C           │
│  Knock: 0     Oil: 4.2 bar          │
│  Advance: 14° (retard: -8°)         │
└─────────────────────────────────────────┘
```

---

## CONTATOS E RECURSOS

### Software
```
TunerStudio:    tunerstudio.com
MegaLogViewer:  megalogviewer.com
SCG-ECU Docs:   (documentação projeto)
```

### Fóruns/Comunidade
```
Speeduino:      speeduino.com/forum
AP Turbo BR:    (grupos Facebook)
```

### Documentação Completa
```
VW_GOL_AP18_COMPLETO.md          (126 KB - tudo!)
VW_GOL_COMPARATIVO_VERSOES.md    (comparativo)
VW_GOL_QUICK_REFERENCE.md        (este arquivo)
```

---

## EMERGÊNCIA - VOLTAR PARA SEGURO

### Tune de Emergência (Limp Mode)
```
Se algo der errado, carregar valores seguros:

VE Table:       80% everywhere
Advance Table:  15° everywhere
AFR Target:     Lambda 0.90 (rico)
Rev Limit:      5000 RPM
Boost Target:   0 bar (turbo)

Isto permite dirigir até oficina!
```

---

**FIM DO QUICK REFERENCE**

**Versão:** 1.0
**Data:** 01/11/2025
**Imprimir:** Deixar na bancada/carro!

```
═══════════════════════════════════════════════════════════════
           KEEP CALM AND TUNE PROGRESSIVELY

    "Rich is safe, lean is mean, knock is GAME OVER"
                 - Tuner Wisdom
═══════════════════════════════════════════════════════════════
```
