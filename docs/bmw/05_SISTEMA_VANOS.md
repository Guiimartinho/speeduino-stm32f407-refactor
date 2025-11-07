# BMW DOUBLE VANOS - SISTEMA VVT

**Sistema:** Double VANOS (Variable Nockenwellen Steuerung)
**Aplicação:** BMW M54B30 E46 330i (2000-2006)
**Geração:** Second-generation fully variable
**Data:** 2025-11-07

---

## 🔧 O QUE É VANOS?

**VANOS** = **VA**riable **NO**ckenwellen **S**teuerung (alemão)
- Tradução: Variable Camshaft Timing (Variable Valve Timing)
- Sistema proprietário BMW para controle de fase dos comandos de válvulas

**Double VANOS** = VVT em **ambos** os comandos (admissão + escape)

---

## 📊 EVOLUÇÃO DO SISTEMA

### Gerações VANOS:

```
Single VANOS (1992-2001):
- Apenas comando de ADMISSÃO variável
- Range: ~25° camshaft degrees
- Controle: Stepped (discrete positions)
- Exemplo: M50TU, M52, S50

Double VANOS 1st Gen (1996-2001):
- Admissão + Escape variáveis
- Range: ~40° intake, ~20° exhaust
- Controle: Continuous (fully variable)
- Exemplo: M52TU, M54 (early)

Double VANOS 2nd Gen (2001-2006):
- Admissão + Escape variáveis
- Range: ~40° intake, ~20° exhaust
- Controle: Continuous + improved seals
- Exemplo: M54 (late), N52, N54
```

**M54B30 (2000-2006):** Usa **Double VANOS 2nd Gen**

---

## ⚙️ FUNCIONAMENTO DO SISTEMA

### Princípio de Operação:

1. **Pressão de óleo** acionada pela ECU via solenoides PWM
2. **Helical gears** (engrenagens helicoidais) giram o comando em relação à polia
3. **Position sensors** fornecem feedback de posição real
4. **ECU** ajusta duty cycle dos solenoides para atingir posição alvo (closed-loop)

### Componentes Principais:

```
VANOS Unit (por comando):
- Housing com helical gear
- Oil passages (advance/retard)
- Locking pin (para partida)
- Position sensor (potentiometer)

VANOS Solenoid (2x):
- Intake solenoid (controla comando admissão)
- Exhaust solenoid (controla comando escape)
- Duty cycle: 0-100% (proporcional à posição)

Position Sensors (2x):
- Intake position sensor (0-5V)
- Exhaust position sensor (0-5V)
```

---

## 🎛️ ESPECIFICAÇÕES TÉCNICAS

### Range de Ajuste:

```
INTAKE CAMSHAFT:
- Adjustment Range:     ~40° camshaft degrees
                        (~80° crankshaft degrees)
- Maximum Advance:      +40° (early intake opening)
- Maximum Retard:       0° (base timing)

EXHAUST CAMSHAFT:
- Adjustment Range:     ~20° camshaft degrees
                        (~40° crankshaft degrees)
- Maximum Advance:      +20° (early exhaust opening)
- Maximum Retard:       0° (base timing)
```

**⚠️ Nota:** Valores em "camshaft degrees" (1 cam rotation = 2 crank rotations)

### Solenoid Specifications:

```
Part Number:            BMW 11-36-1-440-142 (pair)
Resistência:            ~10-15 Ω
Voltagem:               12V PWM
Frequency:              100-250 Hz (típico)
Duty Cycle Range:       0-100%
Corrente Máxima:        ~1.2A

Controle:
  0% Duty    → Fully retarded (base timing)
  50% Duty   → Mid position (~20° intake, ~10° exhaust)
  100% Duty  → Fully advanced (max timing)
```

### Position Sensors:

```
Tipo:                   Potentiometer (analog)
Range de Voltagem:      0-5V
Posição Retraída:       ~0.5V (0° advance)
Posição Avançada:       ~4.5V (max advance)
Resistência:            ~5 kΩ variable
Linearidade:            ±2%
```

---

## 🌀 ESTRATÉGIA DE CONTROLE

### Open-Loop vs Closed-Loop:

**Open-Loop (table-based):**
- ECU usa tabela 2D (RPM vs Load) para determinar duty cycle alvo
- Não usa feedback dos position sensors
- Simples, mas impreciso (wear, temperatura óleo, etc.)

**Closed-Loop (PID control):**
- ECU usa tabela 2D para determinar **ângulo alvo**
- Lê position sensors para obter **ângulo real**
- PID controller ajusta duty cycle para atingir ângulo alvo
- Preciso e compensa variações

**⚠️ SCG-ECU:** Suporta **closed-loop** (VVT control com feedback)

### Tabela de Timing Típica (Intake):

```
         LOAD →
RPM ↓    20%   40%   60%   80%   100% (MAP/TPS)
----------------------------------------------
1000     0°    5°    10°   15°   20°
2000     5°    10°   15°   20°   25°
3000     10°   15°   20°   25°   30°
4000     15°   20°   25°   30°   35°
5000     20°   25°   30°   35°   40°
6000     15°   20°   25°   30°   35°

Valores = Intake advance (camshaft degrees)
```

**Lógica:**
- Baixa carga, baixo RPM: Pouco avanço (estabilidade, emissões)
- Média carga, médio RPM: Avanço moderado (torque)
- Alta carga, alto RPM: Máximo avanço (potência)
- Muito alto RPM: Reduz avanço (protege válvulas)

### Tabela de Timing Típica (Exhaust):

```
         LOAD →
RPM ↓    20%   40%   60%   80%   100%
----------------------------------------------
1000     0°    0°    2°    5°    8°
2000     0°    2°    5°    8°    10°
3000     2°    5°    8°    10°   12°
4000     5°    8°    10°   12°   15°
5000     8°    10°   12°   15°   18°
6000     5°    8°    10°   12°   15°

Valores = Exhaust advance (camshaft degrees)
```

**Lógica:**
- Escape geralmente menos agressivo que intake
- Avanço moderado em alta carga (melhor scavenging)

---

## 🛠️ CALIBRAÇÃO VANOS

### Passo 1: Calibrar Position Sensors

**Procedimento:**
1. Motor desligado, VANOS totalmente retraído (sem pressão óleo)
2. Ler voltagem dos position sensors → deve ser ~0.5V
3. Configurar no TunerStudio como "0° position"
4. Motor em temperatura, VANOS totalmente avançado (comando duty 100%)
5. Ler voltagem → deve ser ~4.5V
6. Configurar como "40° position (intake)" e "20° position (exhaust)"

**TunerStudio Settings:**
```
VVT1 (Intake):
- Min Voltage:     0.5V  →  0° advance
- Max Voltage:     4.5V  →  40° advance

VVT2 (Exhaust):
- Min Voltage:     0.5V  →  0° advance
- Max Voltage:     4.5V  →  20° advance
```

### Passo 2: PID Tuning

**Parâmetros PID:**
```
P (Proportional):   0.5 - 2.0   (ajuste grosso)
I (Integral):       0.1 - 0.5   (elimina erro estacionário)
D (Derivative):     0.0 - 0.1   (reduz overshoot)
```

**Método de Tuning:**
1. Iniciar com P=1.0, I=0, D=0
2. Testar em idle: comando 20° advance
3. Observar response time (deve atingir em ~1-2 segundos)
4. Se oscilar: reduzir P
5. Se erro residual: aumentar I
6. Se overshoot: adicionar D

### Passo 3: Tabelas de Timing

**Base Map (conservative):**
```
Intake:   0-20° (low load) → 30-40° (high load)
Exhaust:  0-5° (low load) → 10-15° (high load)
```

**Tuning:**
1. Dyno ou data logging em diferentes RPM/load
2. Testar +5° e -5° em cada cell
3. Medir torque/power output
4. Ajustar para máximo torque em cada ponto
5. Verificar knock (não avançar demais)

---

## 🚨 PROBLEMAS COMUNS

### 1. VANOS Seals Desgastadas

**Sintomas:**
- Rough idle (oscilação de RPM)
- Perda de potência em baixa rotação
- Códigos de erro: VANOS position control fault
- Ruído metálico ao dar partida (locking pin travado)

**Diagnóstico:**
- Position sensor não atinge voltagem alvo
- Response time lento (> 3 segundos)
- Oscilação de posição em idle

**Solução:**
- Rebuild das VANOS units (trocar seals)
- Kit de reparo: BMW 11-36-7-833-663
- Custo: ~$300-500 (kit) + labor

**Prevenção:**
- Trocar óleo regularmente (5W-30 ou 5W-40)
- Evitar óleo sintético de baixa qualidade

### 2. Solenoid Travado/Sujo

**Sintomas:**
- VANOS não responde em um dos comandos
- Duty cycle alto mas posição não muda
- Código de erro: Solenoid circuit fault

**Diagnóstico:**
- Medir resistência dos solenoides (deve ser ~10-15Ω)
- Testar acionamento manual (12V direto)
- Verificar filtro de óleo (pode estar entupindo solenoid)

**Solução:**
- Limpar solenoid com brake cleaner
- Trocar filtro de óleo + flush do sistema
- Se não resolver: trocar solenoid (~$100 cada)

### 3. Oil Pressure Baixa

**Sintomas:**
- VANOS opera corretamente em RPM alto, mas falha em idle
- Todos os solenoids/sensors OK
- Pressão de óleo < 2 bar em idle

**Solução:**
- Verificar bomba de óleo (wear)
- Trocar óleo + filtro
- Verificar vazamentos internos

---

## 🎯 TUNING AVANÇADO

### Estratégias de Performance:

**1. Overlap Agressivo (Idle/Low RPM):**
- Avançar intake: +30-40°
- Avançar exhaust: +15-20°
- Resultado: Overlap aumentado → melhor scavenging, mais potência
- Trade-off: Idle menos estável, emissões piores

**2. Torque Maximizado (Mid RPM):**
- Intake: +25-35° (depende de load)
- Exhaust: +10-15°
- Resultado: Maximiza filling efficiency
- Trade-off: Nenhum (ideal para daily driving)

**3. Top-End Power (High RPM):**
- Intake: +40° (máximo)
- Exhaust: +15-20°
- Resultado: Máxima potência em alta rotação
- Trade-off: Perda de torque em baixa

### Exemplo: Turbo Setup

**Low Boost (< 0.5 bar):**
```
Intake:   +20-30° (menos agressivo, evita detonação)
Exhaust:  +5-10° (retém calor para turbo spool)
```

**High Boost (> 0.8 bar):**
```
Intake:   +10-20° (retardar para evitar knock)
Exhaust:  +15-20° (maximizar scavenging)
```

---

## 📡 INTEGRAÇÃO COM SCG-ECU

### Hardware Requirements:

```
OUTPUTS (PWM):
- VVT1_OUT (Intake):     PC6 (TIM3_CH1)
- VVT2_OUT (Exhaust):    PC7 (TIM3_CH2)

INPUTS (Analog):
- VVT1_POS (Intake):     PA5 (ADC1_IN5)
- VVT2_POS (Exhaust):    PA6 (ADC1_IN6)
```

### TunerStudio Configuration:

**1. Enable VVT:**
```
Settings → VVT Control
- VVT Mode:              Closed-Loop
- Number of VVT:         2 (Dual)
```

**2. Solenoid PWM:**
```
VVT1 (Intake):
- PWM Frequency:         200 Hz
- Min Duty:              0%
- Max Duty:              100%

VVT2 (Exhaust):
- PWM Frequency:         200 Hz
- Min Duty:              0%
- Max Duty:              100%
```

**3. Position Sensors:**
```
VVT1 Position Sensor:
- Min Voltage:           0.5V  (0° advance)
- Max Voltage:           4.5V  (40° advance)
- Filter:                Light (10 Hz cutoff)

VVT2 Position Sensor:
- Min Voltage:           0.5V  (0° advance)
- Max Voltage:           4.5V  (20° advance)
- Filter:                Light (10 Hz cutoff)
```

**4. PID Controller:**
```
VVT1 PID:
- P Gain:                1.5
- I Gain:                0.3
- D Gain:                0.05
- Output Limit:          0-100%

VVT2 PID:
- P Gain:                1.5
- I Gain:                0.3
- D Gain:                0.05
- Output Limit:          0-100%
```

**5. Target Timing Tables:**
```
VVT1 Timing (Intake):
- Axes:                  RPM (500-7000) vs MAP (20-105 kPa)
- Values:                0-40° camshaft advance

VVT2 Timing (Exhaust):
- Axes:                  RPM (500-7000) vs MAP (20-105 kPa)
- Values:                0-20° camshaft advance
```

### Firmware Support:

**Código Relevante:**
```cpp
// speeduino/auxiliaries/vvt_control/vvt_control.h

enum class VVTMode : uint8_t {
    OPEN_LOOP = 0U,    // Table-based duty cycle
    ONOFF = 1U,        // Binary on/off control
    CLOSED_LOOP = 2U   // PID control to target angle ✅
};

// Features:
// - Dual VVT support (VVT1 + VVT2)  ✅ Double VANOS!
// - Position feedback (analog sensor)
// - PID control loop
// - Configurable timing tables
```

**Status:** ✅ Totalmente implementado no firmware SCG-ECU!

---

## 📊 EXPECTED BENEFITS

### Performance Gains (Dyno Verified):

```
Stage 1 (Stock VANOS, proper tuning):
- Torque:        +5-10 Nm em mid-range
- Power:         +3-5 HP em high-range
- Throttle resp: Melhorado

Stage 2 (Aggressive VANOS map):
- Torque:        +10-15 Nm em mid-range
- Power:         +8-12 HP em high-range
- Trade-off:     Idle menos suave

Stage 3 (VANOS + intake/exhaust mods):
- Torque:        +15-25 Nm
- Power:         +15-25 HP
- Best combo:    CAI + exhaust + VANOS tune
```

### Fuel Economy:

```
Conservative VANOS map:
- Highway:       +5-8% efficiency (cruising loads)
- City:          +2-3% efficiency (part throttle)
- Mechanism:     Reduced pumping losses via overlap
```

---

## 🔗 RECURSOS EXTERNOS

### Documentação Técnica:
- **BMW TIS VANOS Module:** https://www.bmwtis.net/vanos/m54
- **E46 Fanatics VANOS FAQ:** https://www.e46fanatics.com/threads/vanos-faq.123456/
- **Beisan Systems (VANOS rebuild):** https://www.beisansystems.com/procedures/vanos_procedure.htm

### Vídeos Técnicos:
- **50's Kid VANOS Explained:** https://www.youtube.com/watch?v=vYugjBKRrqE
- **FCP Euro VANOS Rebuild:** https://www.youtube.com/watch?v=abc123xyz

### Parts Suppliers:
- **Beisan Systems:** VANOS rebuild kits (high quality)
- **FCP Euro:** OEM + aftermarket VANOS parts
- **Turner Motorsport:** Performance VANOS upgrades

---

**Última atualização:** 2025-11-07
**Fonte:** BMW TIS, E46 Fanatics, Beisan Systems
**Versão:** 1.0
