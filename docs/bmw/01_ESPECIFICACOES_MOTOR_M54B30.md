# BMW M54B30 - ESPECIFICAÇÕES TÉCNICAS COMPLETAS

**Motor:** BMW M54B30
**Aplicação:** BMW 330i/330xi/330Ci E46 (2000-2006)
**Tipo:** 6-cilindros em linha, aspirado
**Fabricação:** Munich Plant, Germany (2000-2006)

---

## 🔧 ESPECIFICAÇÕES BÁSICAS

### Capacidade e Desempenho

```
Displacement:        2,979 cc (2.98 L, 181.8 cu-in)
Potência:            231 PS (170 kW; 228 HP) @ 5,900 RPM
Torque:              300 N·m (30.6 kg·m, 221.1 ft·lb) @ 3,500 RPM
Peso:                130 kg (286.6 lbs)
Potência Específica: 77.5 HP/L
Redline:             6,500 RPM (cut @ 6,750 RPM)
```

### Geometria do Motor

```
Cilindros:           6 em linha
Bore (Diâmetro):     84.0 mm (3.31 in)
Stroke (Curso):      89.6 mm (3.53 in)
Relação Bore/Stroke: 0.938 (undersquare - motor de torque)
Compression Ratio:   10.2:1
Rod Length:          135 mm
Main Bearings:       7 (sete mancais principais)
```

### Firing Order

```
Firing Order:        1-5-3-6-2-4
Cylinder Numbering:  1 = front (belt side), 6 = rear (firewall side)
```

**Sequência de Ignição/Injeção:**
```
Cyl 1 → 0°
Cyl 5 → 120°
Cyl 3 → 240°
Cyl 6 → 360°
Cyl 2 → 480°
Cyl 4 → 600°
```

---

## 🏗️ CONSTRUÇÃO DO MOTOR

### Bloco do Motor

```
Material:            Liga de alumínio fundido
Cylinder Liners:     Ferro fundido (cast-iron liners)
Deck Height:         ~220 mm
Open/Closed Deck:    Open deck
```

### Cabeçote

```
Material:            Alumínio
Valvetrain:          DOHC (Double Overhead Camshaft)
Válvulas/Cilindro:   4 (24 válvulas total)
Intake Valves:       33.0 mm (1.2992 in) diâmetro
Exhaust Valves:      30.5 mm (1.2008 in) diâmetro
Valve Lift:          9.7 mm (0.3818 in) intake, 9.0 mm (0.3543 in) exhaust
Valve Duration:      240° intake, 244° exhaust
```

### Comando de Válvulas

```
Drive Type:          Chain (corrente dupla)
Cam Timing:          Double VANOS (variable intake + exhaust)
Intake Cam:          Variable timing via VANOS
Exhaust Cam:         Variable timing via VANOS
```

**Double VANOS Specs:**
- System Type: Second-generation fully variable
- Adjustment Range: ~40° intake, ~20° exhaust
- Control: Oil pressure actuated, ECU controlled
- Part Number: BMW 11-36-1-440-142

---

## ⚙️ SISTEMA DE GERENCIAMENTO

### ECU Original

```
ECU Model:           Siemens MS43
Generation:          Bosch ME7.x derivative
Processor:           Infineon TriCore (32-bit)
Memory:              Flash + EEPROM
CAN-Bus:             500 kbps, PT-CAN (Powertrain CAN)
```

### Sistema de Injeção

```
Tipo:                Sequential multi-point fuel injection (SFI)
Injetores:           6x Bosch individual
Flow Rate:           236 cc/min @ 3 bar (OEM)
                     282 cc/min @ 3 bar (aftermarket spec)
Resistência:         11.7-14.9 Ω (nominal 12-13Ω)
Part Number:         BMW 13537546245 (OEM)
Pressão:             3.5 bar (51 PSI) nominal
Controle:            PWM, saturated type (high impedance)
Dead Time:           ~0.8 ms @ 14V
```

**Injetor Upgrade (turbo):**
```
Flow Rate:           440 cc/min @ 3 bar
                     600 cc/min @ 4 bar
Exemplos:            Bosch EV14, Injector Dynamics ID1000
```

### Sistema de Ignição

```
Tipo:                Coil-on-Plug (COP) distributorless
Bobinas:             6x individuais
Tipo de Bobina:      Pencil coil (stick coil)
Part Number:         Bosch 0 221 504 470 (OEM)
Resistência Pri:     0.8 Ω (between pins 1 and 15)
Resistência Sec:     1.0 kΩ ±20% (old style), 1.8 kΩ ±20% (new style)
Dwell:               3.0-3.5 ms @ 14V
Spark Plug Gap:      0.9-1.0 mm (0.035-0.040 in)
Spark Plugs:         NGK BCPR6ES-11, Bosch FR7LDC+
```

### Sistema de Admissão

```
Throttle:            Electronic throttle (drive-by-wire)
Throttle Body:       60mm (2.36 in) diameter
TPS Sensor:          Dual potentiometer (redundant)
TPS Voltage:         0-5V (pin 1), signal on pin 3
TPS Resistance:      ~2-5 kΩ variable
```

**DISA (Dual Intake System Adjustment):**
```
Sistema:             Coletor de admissão com comprimento variável
Tipo:                Flapper valve interno
Acionamento:         Vácuo eletro-pneumático
Condição LOW RPM:    Long runners (performance em baixa)
Condição HIGH RPM:   Short runners (performance em alta)
Transição:           ~3,500 RPM
```

**MAF Sensor (Mass Air Flow):**
```
Tipo:                Hot-wire anemometer
Range:              0-5V
Idle Flow:          ~2-4 g/s
WOT Flow:           ~180-200 g/s @ 6000 RPM
Part Number:        BMW 13621432356 (Bosch 0 280 217 814)
```

---

## 🌡️ SENSORES

### Sensor de Temperatura do Motor (CLT)

```
Localização:         Cabeçote, lado esquerdo frontal
Tipo:                NTC thermistor
Resistência:         ~4500Ω @ 6°C
                     ~1100Ω @ 42°C
                     ~270Ω @ 86°C
Voltagem:            0.05V / 5.05V nos terminais
Part Number:         BMW 13621433077
```

### Sensor de Temperatura do Ar (IAT)

```
Localização 2001-03/2003:  Entre runners 3 e 4 (topo do motor)
Localização 04/2003+:      Integrado no MAF (conector 6-pin)
Tipo:                      NTC thermistor (similar ao CLT)
Part Number:               BMW 13621747155 (standalone)
```

### Sensor de Posição do Virabrequim (CKP)

```
Localização:         Lado direito do bloco, próximo ao volante
Tipo:                Hall effect sensor
Trigger Wheel:       60-2 (60 dentes, 2 faltando)
Gap:                 ~1-2 mm
Missing Teeth:       Position = 90° BTDC cyl #1
Part Number:         BMW 12141247978
```

### Sensor de Posição do Comando (CMP)

```
Sensores:            2x (intake + exhaust)
Localização Intake:  Frente do cabeçote, lado intake
Localização Exhaust: Traseira do cabeçote, lado exhaust
Tipo:                Hall effect sensor
Trigger:             1 pulse/revolution
Part Number:         BMW 12141438082
```

### Sensor de Detonação (Knock Sensor)

```
Quantidade:          2x
Localização:         Bloco do motor, entre cilindros 2-3 e 4-5
Tipo:                Piezoelectric
Frequency Range:     5-15 kHz
Part Number:         BMW 13627537453
```

### Sensor Lambda (O2)

```
Sensores:            2x (upstream) + 2x (downstream)
Upstream Position:   Antes dos catalisadores (1 por banco)
Downstream Position: Depois dos catalisadores (monitor)
Tipo Upstream:       Wideband LSU 4.2 (Bosch)
Tipo Downstream:     Narrowband (switching sensor)
Part Number (WBO2):  Bosch 0 258 007 057 (BMW 11781427884)
```

**⚠️ Nota para SCG-ECU:**
- SCG-ECU tem wideband controller integrado (LSU 4.9 compatible)
- Pode usar sensor BMW OEM ou aftermarket LSU 4.9
- Recomendado: Bosch LSU 4.9 (mais preciso, mesmo tipo da SCG-ECU)

---

## 💨 SISTEMA DE INDUÇÃO E EXAUSTÃO

### Sistema de Admissão

```
Tipo:                Naturally aspirated
Air Filter:          K&N 33-2231 (aftermarket popular)
Intake Manifold:     Dual-length (DISA system)
Throttle Body:       Electronic 60mm
```

### Sistema de Exaustão

```
Manifolds:           Cast iron, 3-into-1 design (2x)
Catalytic Converters: 2x primary cats + 2x secondary cats
Oxygen Sensors:      4x total (2 upstream WBO2 + 2 downstream NBO2)
Muffler:             Dual outlet (stock)
```

---

## 🔩 COMPONENTES INTERNOS

### Pistões

```
Material:            Aluminum alloy
Compression Height:  ~31 mm
Pins:                Fully floating
Rings:               3 (2 compression + 1 oil)
```

### Bielas

```
Material:            Forged steel
Length:              135 mm center-to-center
Big End:             Cracked cap (fracture-split)
Bolts:               Torque-to-yield (TTY) - usar novos sempre
```

### Virabrequim

```
Material:            Forged steel
Main Journals:       7
Stroke:              89.6 mm
Balanceamento:       Internally balanced
```

### Comando de Válvulas

```
Material:            Chilled cast iron
Lobes:               Asymmetric profile
Lifters:             Hydraulic bucket type
Shims:               Not required (auto-adjusting)
```

---

## 🛢️ LUBRIFICAÇÃO

### Sistema de Óleo

```
Tipo:                Wet sump
Capacidade:          6.5 L (6.9 qt) com filtro
Óleo Recomendado:    5W-30, 5W-40 (BMW LL-01 approved)
Pressão Idle:        1.0 bar @ 900 RPM
Pressão Normal:      2.0-4.5 bar @ 3000 RPM
Filtro:              BMW 11427512300 (OEM Mahle/Mann)
```

### Sensor de Pressão de Óleo

```
Localização:         Bloco inferior, lado esquerdo
Tipo:                Analog voltage output
Range:               0-10 bar
Part Number:         BMW 12611439810
```

---

## 💧 ARREFECIMENTO

### Sistema de Refrigeração

```
Capacidade Total:    ~9.0 L (incluindo heater)
Termostato:          Eletrônico, multi-stage
Temp Abertura:       88°C (primeira abertura), 100°C (totalmente aberto)
Radiador:            Aluminum, cross-flow
Ventilador:          Elétrico, PWM controlled
Part Number Thermo:  BMW 11531437040
```

### Bomba D'água

```
Drive Type:          Belt-driven (serpentine belt)
Flow Rate:           ~120 L/min @ 5000 RPM
Part Number:         BMW 11511436386
```

---

## ⚡ SISTEMA ELÉTRICO

### Alternador

```
Voltage:             14V nominal
Current:             120A (140A em alguns modelos)
Control:             LIN-Bus voltage regulation
Part Number:         Bosch 0 124 515 089
```

### Motor de Partida

```
Voltage:             12V
Power:               1.4 kW
Part Number:         Bosch 0 001 107 462
```

---

## 📊 PERFORMANCE SPECS (BMW 330i E46)

### Aceleração

```
0-60 mph:            6.5 segundos (manual), 7.0 segundos (auto)
0-100 km/h:          6.7 segundos (manual), 7.2 segundos (auto)
1/4 mile:            14.9 segundos @ 95 mph
Top Speed:           250 km/h (155 mph) - limitado eletronicamente
```

### Consumo de Combustível (EPA)

```
City:                19 mpg (12.4 L/100km)
Highway:             28 mpg (8.4 L/100km)
Combined:            23 mpg (10.2 L/100km)
```

### Emissões

```
Padrão:              Euro 3 (2000-2003), Euro 4 (2004-2006)
CO:                  < 2.3 g/km
HC:                  < 0.2 g/km
NOx:                 < 0.15 g/km
```

---

## 🔧 MANUTENÇÃO

### Intervalos de Serviço BMW

```
Óleo e Filtro:       15,000 km ou 1 ano (recomendado: 10,000 km)
Filtro de Ar:        30,000 km
Velas:               60,000 km
Correia Serpentina:  90,000 km
Correia Comando:     150,000 km (crítico!)
Bomba D'água:        150,000 km (fazer junto com correia)
VANOS:               Rebuild @ 150,000 km (se apresentar problemas)
```

### Torques Importantes

```
Spark Plugs:         30 Nm (22 ft-lb)
Oil Drain Plug:      25 Nm (18 ft-lb)
Oil Filter Housing:  25 Nm (18 ft-lb)
Valve Cover:         10 Nm (7 ft-lb)
Intake Manifold:     10 Nm (7 ft-lb)
Exhaust Manifold:    20 Nm (15 ft-lb) + 90°
```

---

## 🚨 PROBLEMAS COMUNS

### Conhecidos do M54B30

1. **VANOS Seals:** Desgaste aos 100-150k km, causa rough idle
2. **CCV (Crankcase Vent Valve):** Entupimento, causa consumo de óleo
3. **Oil Filter Housing Gasket:** Vazamento comum aos 80-100k km
4. **Cooling System:** Plásticos frágeis (radiador, expansion tank)
5. **Window Regulator:** Falha mecânica (não é motor, mas comum E46)

---

## 💡 POTENCIAL DE TUNING

### Aspirado (NA)

```
Stage 1 (Tune Only):
- Ganho: +15-20 HP, +10-15 Nm
- Mods: Apenas remap ECU
- Custo: Baixo

Stage 2 (Bolt-on):
- Ganho: +25-35 HP, +20-25 Nm
- Mods: Intake, exhaust, tune
- Custo: Médio

Stage 3 (Cabeçote + Comando):
- Ganho: +50-70 HP
- Mods: Port/polish, cam upgrade, tune
- Custo: Alto
```

### Forçado (Turbo/Supercharger)

```
Turbo (Single):
- Potência: 350-400 HP @ 0.8 bar
- Potência: 450-500 HP @ 1.2 bar
- Limitação: Internos (pistões, bielas)
- Safe Limit: ~400 HP com forjados

Supercharger:
- Potência: 300-350 HP
- Vantagem: Linear delivery, sem lag
- Exemplo: ESS VT2-650 kit
```

---

## 📁 RECURSOS ADICIONAIS

### Manuais e Documentação

- BMW TIS (Technical Information System)
- Bentley Service Manual E46
- Haynes Repair Manual 18023

### Comunidades

- E46 Fanatics Forum (maior comunidade)
- Bimmerforums (discussões técnicas)
- M54 Facebook Groups

---

**Última atualização:** 2025-11-07
**Fontes:** BMW TIS, MS4X Wiki, Engine-Specs.net, E46 Fanatics
**Versão:** 1.0
