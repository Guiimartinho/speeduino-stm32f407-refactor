# SCG-ECU 2.0 - CAN Dashboard Master Reference

## Resumo do Sistema

| Item | Valor |
|------|-------|
| **MCU** | STM32F407VGT6 (ARM Cortex-M4 @ 168MHz) |
| **CAN Bus** | 500 kbps, ISO 11898, CAN 2.0B |
| **Pinos CAN** | PD0 (RX), PD1 (TX) |
| **Configuracao** | 8x8 (8 injetores + 8 ignicoes) |

---

## PARTE 1: ECU → DASHBOARD (TX)

### 1.1 Protocolo BMW (E46/E39/E38)

#### 0x316 - DME1 (30Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0 | Status flags | `0x05` fixo | bitfield |
| 1 | Torque indexado | `0x0C` fixo | % |
| 2-3 | **RPM** | `RPM × 6.4` (LSB first) | rpm |
| 4 | Torque indicado | `0x0C` fixo | % |
| 5 | Perda torque | `0x15` fixo | % |
| 6 | Nao usado | `0x00` | - |
| 7 | Torque teorico | `0x35` fixo | % |

**Decodificacao Dashboard:**
```c
uint16_t rpm = ((buf[3] << 8) | buf[2]) / 6.4;
```

#### 0x329 - DME2 (30Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0 | Info multiplexada | `0x11` fixo | - |
| 1 | **Temperatura agua** | `((CLT + 48) × 4) / 3` | graus |
| 2 | **Pressao baro** | Valor direto | kPa |
| 3 | Status flags | `0x08` = motor rodando | bitfield |
| 4 | TPS virtual cruise | `0x00` nao usado | - |
| 5 | **TPS** | `map(TPS, 0, 200, 1, 254)` | 0-254 |
| 6 | Status freio | `0x00` | bitfield |
| 7 | Nao usado | `0x00` | - |

**Decodificacao Dashboard:**
```c
int8_t coolant = ((buf[1] * 3) / 4) - 48;  // Em graus Celsius
uint8_t tps_pct = map(buf[5], 1, 254, 0, 100);
uint8_t baro = buf[2];  // kPa direto
```

#### 0x545 - DME4 (10Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0 | **Status CEL/MIL** | Bit1 = MIL | bitfield |
| 1-2 | **Consumo combustivel** | LSB first, × 0.01 | L/h |
| 3 | **Overheat warning** | `0x08` se CLT > 120C | flag |
| 4 | **Temp oleo** | `(valor × 0.75) - 48` | graus |

**Decodificacao Dashboard:**
```c
bool mil_on = (buf[0] & 0x02) != 0;
float fuel_Lh = ((buf[2] << 8) | buf[1]) / 100.0;
bool overheat = (buf[3] == 0x08);
int8_t oil_temp = (buf[4] * 3 / 4) - 48;
```

---

### 1.2 Protocolo VAG (VW/Audi)

#### 0x280 - RPM (30Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0 | Header | `0x49` fixo | - |
| 1 | Header | `0x0E` fixo | - |
| 2-3 | **RPM** | `RPM × 4` (LSB first) | rpm |
| 4 | Footer | `0x0E` fixo | - |
| 5 | Footer | `0x00` | - |
| 6 | Footer | `0x1B` fixo | - |
| 7 | Footer | `0x0E` fixo | - |

**Decodificacao Dashboard:**
```c
uint16_t rpm = ((buf[3] << 8) | buf[2]) / 4;
```

#### 0x5A0 - Velocidade (30Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0 | Header | `0xFF` fixo | - |
| 1-2 | **VSS** | `VSS × 133` (LSB first) | km/h |
| 3-6 | Nao usado | `0x00` | - |
| 7 | Footer | `0xAD` fixo | - |

**Decodificacao Dashboard:**
```c
uint16_t vss = ((buf[2] << 8) | buf[1]) / 133;
```

---

### 1.3 Protocolo Haltech (IC-7/IC-10)

**Nota:** Haltech usa Big Endian (MSB first) em todos os campos!

#### 0x360 - DATA1 (50Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0-1 | **RPM** | Valor direto (MSB first) | rpm |
| 2-3 | **MAP** | `× 0.1` | kPa |
| 4-5 | **TPS** | `× 0.2` | % |
| 6-7 | Coolant pressure | Nao implementado | kPa |

**Decodificacao Dashboard:**
```c
uint16_t rpm = (buf[0] << 8) | buf[1];
float map_kpa = ((buf[2] << 8) | buf[3]) / 10.0;
float tps_pct = ((buf[4] << 8) | buf[5]) / 5.0;
```

#### 0x361 - DATA2 (50Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0-1 | **Fuel pressure** | `(valor - 1013) / 68.94` | PSI |
| 2-3 | **Oil pressure** | `(valor - 1013) / 68.94` | PSI |
| 4-5 | **Fuel load** | `× 0.1` | % |
| 6-7 | Wastegate pressure | Nao implementado | kPa |

**Decodificacao Dashboard:**
```c
// Haltech envia em kPa × 10 com offset de 1 atm (1013)
float fuel_psi = (((buf[0] << 8) | buf[1]) - 1013) / 68.94;
float oil_psi = (((buf[2] << 8) | buf[3]) - 1013) / 68.94;
float load_pct = ((buf[4] << 8) | buf[5]) / 10.0;
```

#### 0x362 - DATA3 (50Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0-1 | **Duty cycle inj** | `× 0.1` | % |
| 2-3 | Staging duty | Nao implementado | % |
| 4-5 | **Avanco ignicao** | `× 0.1` (SIGNED!) | graus |
| 6-7 | Nao usado | `0x00` | - |

**Decodificacao Dashboard:**
```c
float duty_pct = ((buf[0] << 8) | buf[1]) / 10.0;
int16_t advance_raw = (int16_t)((buf[4] << 8) | buf[5]);
float advance_deg = advance_raw / 10.0;
```

#### 0x364 - PW Injetores (50Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0-1 | **PW1** | Valor direto | us |
| 2-3 | **PW2** | Valor direto | us |
| 4-5 | **PW3** | Valor direto | us |
| 6-7 | **PW4** | Valor direto | us |

**Decodificacao Dashboard:**
```c
uint16_t pw1_us = (buf[0] << 8) | buf[1];
uint16_t pw2_us = (buf[2] << 8) | buf[3];
uint16_t pw3_us = (buf[4] << 8) | buf[5];
uint16_t pw4_us = (buf[6] << 8) | buf[7];
```

#### 0x368 - LAMBDA (15Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0-1 | **Lambda 1** | `valor / 1000` | lambda |
| 2-3 | **Lambda 2** | `valor / 1000` | lambda |
| 4-5 | Lambda 3 | Nao implementado | - |
| 6-7 | Lambda 4 | Nao implementado | - |

**Decodificacao Dashboard:**
```c
float lambda1 = ((buf[0] << 8) | buf[1]) / 1000.0;
float afr1 = lambda1 * 14.7;  // Para gasolina
```

#### 0x369 - TRIGGER (15Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0-7 | Sync status | Stub (nao implementado) | - |

#### 0x370 - VSS/VVT (15Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0-1 | **VSS** | `× 0.1` | km/h |
| 2 | Nao usado | `0x00` | - |
| 3 | **Marcha atual** | Valor direto | 0-6 |
| 4-5 | **VVT1 angulo** | `× 0.1` | graus |
| 6-7 | **VVT2 angulo** | `× 0.1` | graus |

**Decodificacao Dashboard:**
```c
float vss_kmh = ((buf[0] << 8) | buf[1]) / 10.0;
uint8_t gear = buf[3];
float vvt1_deg = ((buf[4] << 8) | buf[5]) / 10.0;
float vvt2_deg = ((buf[6] << 8) | buf[7]) / 10.0;
```

#### 0x372 - DATA4 (10Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0 | Battery (high) | `0x00` sempre | - |
| 1 | **Battery** | `× 0.1` | V |
| 2-3 | Nao usado | `0x00` | - |
| 4-5 | **Boost target** | `× 0.1` | kPa |
| 6-7 | **Baro** | `× 0.1` | kPa |

**Decodificacao Dashboard:**
```c
float battery_v = buf[1] / 10.0;
float boost_target_kpa = ((buf[4] << 8) | buf[5]) / 10.0;
float baro_kpa = ((buf[6] << 8) | buf[7]) / 10.0;
```

#### 0x3E0 - DATA5 (10Hz)
| Byte | Dado | Formula | Unidade |
|------|------|---------|---------|
| 0-1 | **CLT** | `(valor / 10) - 273` | Celsius |
| 2-3 | **IAT** | `(valor / 10) - 273` | Celsius |
| 4-5 | **Fuel temp** | `(valor / 10) - 273` | Celsius |
| 6-7 | Oil temp | Nao implementado | Kelvin |

**Decodificacao Dashboard:**
```c
// Haltech envia em Kelvin × 10
int16_t clt_c = (((buf[0] << 8) | buf[1]) / 10) - 273;
int16_t iat_c = (((buf[2] << 8) | buf[3]) / 10) - 273;
int16_t fuel_temp_c = (((buf[4] << 8) | buf[5]) / 10) - 273;
```

---

## PARTE 2: DASHBOARD → ECU (RX)

### 2.1 Wideband O2 - RusEFI

**Receber dados Lambda:**
| CAN ID | Descricao |
|--------|-----------|
| 0x190 | Lambda sensor 1 (O2 primario) |
| 0x192 | Lambda sensor 2 (O2 secundario) |

| Byte | Dado | Condicao |
|------|------|----------|
| 1 | Validity | `0x01` = dado valido |
| 2-3 | **Lambda raw** | LSB first |

**Envio para Dashboard (Heartbeat):**
| Campo | Valor |
|-------|-------|
| CAN ID | `0xEF50000` (EXTENDED!) |
| Byte 0 | `battery10` (tensao × 10) |
| Byte 1 | `0x01` se motor rodando, `0x00` se nao |

**Codigo Dashboard (enviar heartbeat):**
```c
CAN_message_t msg;
msg.id = 0xEF50000;
msg.flags.extended = 1;  // IMPORTANTE!
msg.len = 2;
msg.buf[0] = battery_voltage_x10;  // Ex: 125 = 12.5V
msg.buf[1] = engine_running ? 0x01 : 0x00;
CAN_send(&msg);
```

### 2.2 Wideband O2 - AEM X-Series

| CAN ID | Descricao |
|--------|-----------|
| 0x180 | AEM X-Series UEGO |

| Byte | Dado | Condicao |
|------|------|----------|
| 6 | Validity | Bit 7 = 1 = valido |
| 0-1 | **AFR raw** | MSB first |

### 2.3 AUX CAN Inputs (16 canais)

A ECU pode receber ate 16 canais de dados auxiliares do dashboard:

| Canal | CAN ID Base | Offset |
|-------|-------------|--------|
| 0-15 | `configPage9.caninput_source_can_address[n]` | `+ 0x100` |

**Configuracao TunerStudio:**
- `caninput_source_can_address[0-15]` - Endereco base
- `caninput_source_start_byte[0-15]` - Byte inicial no frame
- `caninput_source_num_bytes` - Bitfield (1 ou 2 bytes por canal)
- `caninputEndianess` - 0=Big Endian, 1=Little Endian

**Dados armazenados em:** `currentStatus.canin[0-15]`

**Exemplo de uso (Dashboard → ECU):**
```c
// Enviar dado para canal AUX 0 (ex: pressao do turbo externo)
CAN_message_t msg;
msg.id = 0x100 + configured_address;  // Ex: 0x100 + 0x50 = 0x150
msg.len = 8;
msg.buf[0] = lowByte(turbo_pressure);
msg.buf[1] = highByte(turbo_pressure);
// ... preencher resto
CAN_send(&msg);
```

---

## PARTE 3: OBD-II (ECU ↔ SCANNER/DASHBOARD)

### 3.1 Enderecos

| Tipo | CAN ID |
|------|--------|
| Request broadcast | 0x7DF |
| Request ECU-specific | `configPage9.obd_address + 0x100` |
| Response | 0x7E8 |

### 3.2 Mode 01 - Dados em Tempo Real

| PID | Descricao | Formula Dashboard |
|-----|-----------|-------------------|
| 0x00 | PIDs suportados 01-20 | Bitfield |
| 0x05 | Temp agua | `A - 40` (Celsius) |
| 0x0A | Pressao combustivel | `A × 3` (kPa) |
| 0x0B | MAP | `A` direto (kPa) |
| 0x0C | RPM | `(256×A + B) / 4` |
| 0x0D | Velocidade | `A` direto (km/h) |
| 0x0E | Avanco ignicao | `(A / 2) - 64` (graus BTDC) |
| 0x0F | Temp ar admissao | `A - 40` (Celsius) |
| 0x11 | TPS | `(A × 100) / 255` (%) |
| 0x13 | Sensores O2 presentes | Bitfield |
| 0x1C | Padrao OBD | 7 = OBD2/EOBD |
| 0x20 | PIDs suportados 21-40 | Bitfield |
| 0x24 | O2 sensor 1 (equiv ratio) | Complexo |
| 0x25 | O2 sensor 2 (equiv ratio) | Complexo |
| 0x33 | Pressao barometrica | `A` direto (kPa) |
| 0x40 | PIDs suportados 41-60 | Bitfield |
| 0x42 | Tensao bateria | `(256×A + B) / 1000` (V) |
| 0x46 | Temp ambiente | `A - 40` (Celsius) |
| 0x52 | Etanol % | `(A × 100) / 255` (%) |
| 0x5C | Temp oleo | `A - 40` (Celsius) |
| 0x60 | PIDs suportados 61-80 | Bitfield |

**Exemplo Request (Dashboard → ECU):**
```c
// Requisitar RPM (PID 0x0C)
CAN_message_t msg;
msg.id = 0x7DF;  // Broadcast
msg.len = 8;
msg.buf[0] = 0x02;  // 2 bytes de dados
msg.buf[1] = 0x01;  // Mode 01
msg.buf[2] = 0x0C;  // PID RPM
msg.buf[3] = 0x00;
// ...
CAN_send(&msg);
```

**Exemplo Response (ECU → Dashboard):**
```c
// Resposta RPM = 3000
// buf[0] = 0x04 (4 bytes)
// buf[1] = 0x41 (Mode 01 response)
// buf[2] = 0x0C (PID)
// buf[3] = 0x2E (high byte: 3000×4 = 12000 = 0x2EE0)
// buf[4] = 0xE0 (low byte)
uint16_t rpm = ((buf[3] << 8) | buf[4]) / 4;  // = 3000
```

### 3.3 Mode 03 - Ler DTCs Confirmados

**Request:**
```c
msg.buf[0] = 0x01;
msg.buf[1] = 0x03;
```

**Response (ex: 2 DTCs):**
```c
// buf[0] = 0x06 (6 bytes)
// buf[1] = 0x43 (Mode 03 response)
// buf[2] = 0x02 (2 DTCs)
// buf[3-4] = DTC 1 (ex: 0x0105 = P0105)
// buf[5-6] = DTC 2
```

**DTCs Definidos:**
| Codigo | Descricao |
|--------|-----------|
| P0105 | MAP sensor malfunction |
| P0107 | MAP sensor low input |
| P0108 | MAP sensor high input |
| P0110 | IAT sensor circuit malfunction |
| P0112 | IAT sensor low input |
| P0113 | IAT sensor high input |
| P0115 | CLT sensor circuit malfunction |
| P0117 | CLT sensor low input |
| P0118 | CLT sensor high input |
| P0120 | TPS circuit malfunction |
| P0130 | O2 sensor circuit malfunction |
| P0171 | System too lean (Bank 1) |
| P0172 | System too rich (Bank 1) |
| P0217 | Engine overtemp condition |
| P0219 | Engine overspeed condition |
| P0230 | Fuel pump primary circuit |
| P0335 | Crank position sensor A circuit |
| P0336 | Crank position sensor A range |
| P0340 | Cam position sensor A circuit |
| P0520 | Engine oil pressure sensor |
| P0562 | System voltage low |
| P0563 | System voltage high |
| P1000 | OBD system readiness not complete |

### 3.4 Mode 04 - Limpar DTCs

**Request:**
```c
msg.buf[0] = 0x01;
msg.buf[1] = 0x04;
```

**Response:**
```c
// buf[0] = 0x01
// buf[1] = 0x44 (Mode 04 response)
```

### 3.5 Mode 07 - Ler DTCs Pendentes

Mesmo formato do Mode 03, mas response = `0x47`

### 3.6 Mode 09 - Informacoes do Veiculo

| PID | Descricao |
|-----|-----------|
| 0x02 | VIN (17 chars, multi-frame) |
| 0x0A | Nome ECU (20 chars, multi-frame) |

### 3.7 Mode 22 - PIDs Customizados

**Canais AUX (0x77xx):**
```c
// Ler canal AUX 1
msg.buf[1] = 0x22;
msg.buf[2] = 0x01;  // Canal 1
msg.buf[3] = 0x77;  // Prefixo AUX
```

**Dados currentStatus (0x78xx):**
```c
// Ler qualquer campo do currentStatus pelo indice
msg.buf[1] = 0x22;
msg.buf[2] = indice;  // Indice do campo
msg.buf[3] = 0x78;    // Prefixo currentStatus
```

---

## PARTE 4: DADOS NAO IMPLEMENTADOS (SUGESTOES FUTURAS)

### 4.1 IDs Customizados Sugeridos (0x400-0x41F)

Para dados que existem no `currentStatus` mas nao sao transmitidos:

#### 0x400 - Derivadas RPM/MAP
| Byte | Dado | Tipo |
|------|------|------|
| 0-1 | rpmDOT | int16_t (RPM/s) |
| 2-3 | mapDOT | int16_t (kPa/s) |
| 4-5 | tpsDOT | int16_t (%/s) |
| 6-7 | Reservado | - |

#### 0x401 - Avanco Detalhado
| Byte | Dado | Tipo |
|------|------|------|
| 0 | advance | int8_t (graus) |
| 1 | advance1 | int8_t (tabela 1) |
| 2 | advance2 | int8_t (tabela 2) |
| 3 | knockRetard | uint8_t |
| 4 | knockCount | uint8_t |
| 5-7 | Reservado | - |

#### 0x402 - Dwell
| Byte | Dado | Tipo |
|------|------|------|
| 0-1 | dwell | uint16_t (ms × 10) |
| 2-3 | actualDwell | uint16_t (us) |
| 4 | dwellCorrection | uint8_t (%) |
| 5-7 | Reservado | - |

#### 0x405 - Correcoes Combustivel 1
| Byte | Dado | Tipo |
|------|------|------|
| 0 | egoCorrection | uint8_t (%) |
| 1 | wueCorrection | uint8_t (%) |
| 2 | batCorrection | uint8_t (%) |
| 3 | iatCorrection | uint8_t (%) |
| 4 | baroCorrection | uint8_t (%) |
| 5 | launchCorrection | uint8_t (%) |
| 6-7 | Reservado | - |

#### 0x406 - Correcoes Combustivel 2
| Byte | Dado | Tipo |
|------|------|------|
| 0 | flexCorrection | uint8_t (%) |
| 1 | fuelTempCorrection | uint8_t (%) |
| 2 | flexIgnCorrection | int8_t (graus) |
| 3 | ASEValue | uint8_t (%) |
| 4-5 | AEamount | uint16_t (%) |
| 6-7 | corrections (total) | uint16_t (%) |

#### 0x408 - VE Tables
| Byte | Dado | Tipo |
|------|------|------|
| 0 | VE | uint8_t (%) |
| 1 | VE1 | uint8_t (%) |
| 2 | VE2 | uint8_t (%) |
| 3 | afrTarget | uint8_t (AFR × 10) |
| 4-5 | fuelLoad | uint16_t |
| 6-7 | ignLoad | uint16_t |

#### 0x410 - Status Launch/Flatshift
| Byte | Dado | Tipo |
|------|------|------|
| 0 | launchingSoft | bool |
| 1 | launchingHard | bool |
| 2 | flatShiftingHard | bool |
| 3-4 | clutchEngagedRPM | uint16_t |
| 5-7 | Reservado | - |

#### 0x413 - Status Flags
| Byte | Dado | Tipo |
|------|------|------|
| 0 | status1 | uint8_t (bitfield) |
| 1 | status2 | uint8_t (bitfield) |
| 2 | status3 | uint8_t (bitfield) |
| 3 | status4 | uint8_t (bitfield) |
| 4 | status5 | uint8_t (bitfield) |
| 5 | engine | uint8_t (bitfield) |
| 6 | airConStatus | uint8_t (bitfield) |
| 7 | engineProtectStatus | uint8_t |

#### 0x416 - VVT Duty Cycles
| Byte | Dado | Tipo |
|------|------|------|
| 0-1 | vvt1Duty | int16_t (%) |
| 2 | vvt1TargetAngle | uint8_t (graus) |
| 3-4 | vvt2Duty | int16_t (%) |
| 5 | vvt2TargetAngle | uint8_t (graus) |
| 6-7 | Reservado | - |

#### 0x365 - PW5-PW8 (Extensao Haltech)
| Byte | Dado | Tipo |
|------|------|------|
| 0-1 | PW5 | uint16_t (us) |
| 2-3 | PW6 | uint16_t (us) |
| 4-5 | PW7 | uint16_t (us) |
| 6-7 | PW8 | uint16_t (us) |

---

## PARTE 5: BITS DE STATUS

### 5.1 engine (BIT_ENGINE_*)
| Bit | Nome | Descricao |
|-----|------|-----------|
| 0 | RUN | Motor rodando |
| 1 | CRANK | Motor em partida |
| 2 | ASE | After Start Enrichment ativo |
| 3 | WARMUP | Motor em aquecimento |
| 4 | ACC | Aceleracao TPS ativa |
| 5 | DCC | Desaceleracao TPS ativa |
| 6 | MAPACC | Aceleracao MAP ativa |
| 7 | MAPDCC | Desaceleracao MAP ativa |

### 5.2 status1 (BIT_STATUS1_*)
| Bit | Nome | Descricao |
|-----|------|-----------|
| 0 | INJ1 | Injetor 1 ativo |
| 1 | INJ2 | Injetor 2 ativo |
| 2 | INJ3 | Injetor 3 ativo |
| 3 | INJ4 | Injetor 4 ativo |
| 4 | DFCO | Fuel cut-off em desaceleracao |
| 5 | BOOSTCUT | Boost cut (combustivel) |
| 6 | TOOTHLOG1READY | Log dentes 1 pronto |
| 7 | TOOTHLOG2READY | Log dentes 2 pronto |

### 5.3 status2 (BIT_STATUS2_*)
| Bit | Nome | Descricao |
|-----|------|-----------|
| 0 | HLAUNCH | Hard launch ativo |
| 1 | SLAUNCH | Soft launch ativo |
| 2 | HRDLIM | Hard limiter ativo |
| 3 | SFTLIM | Soft limiter ativo |
| 4 | BOOSTCUT | Boost cut (ignicao) |
| 5 | ERROR | Erro detectado |
| 6 | IDLE | Modo marcha lenta |
| 7 | SYNC | Motor sincronizado |

### 5.4 status5 (BIT_STATUS5_*)
| Bit | Nome | Descricao |
|-----|------|-----------|
| 0 | FLATSH | Flat shift hard cut |
| 1 | FLATSS | Flat shift soft cut |
| 2 | SPARK2_ACTIVE | Tabela ignicao 2 ativa |
| 3 | KNOCK_ACTIVE | Sistema knock ativo |
| 4 | KNOCK_PULSE | Pulso knock detectado |
| 5 | CLUTCH_PRESS | Embreagem pressionada |
| 6 | SENSOR_CLT | Falha sensor CLT |
| 7 | SENSOR_IAT | Falha sensor IAT |

### 5.5 airConStatus (BIT_AIRCON_*)
| Bit | Nome | Descricao |
|-----|------|-----------|
| 0 | REQUEST | Botao A/C pressionado |
| 1 | COMPRESSOR | Compressor rodando |
| 2 | RPM_LOCKOUT | Bloqueio por RPM |
| 3 | TPS_LOCKOUT | Bloqueio por TPS |
| 4 | TURNING_ON | A/C ligando (delay) |
| 5 | CLT_LOCKOUT | Bloqueio por temp agua |
| 6 | FAN | Ventoinha A/C ativa |

### 5.6 engineProtectStatus (ENGINE_PROTECT_BIT_*)
| Bit | Nome | Descricao |
|-----|------|-----------|
| 0 | RPM | Protecao RPM ativa |
| 1 | MAP | Protecao MAP ativa |
| 2 | OIL | Protecao oleo ativa |
| 3 | AFR | Protecao AFR ativa |
| 4 | COOLANT | Protecao temp agua ativa |

---

## PARTE 6: FREEZE FRAME

Quando um DTC confirmado e armazenado, a ECU captura um snapshot:

| Campo | Tipo | Descricao |
|-------|------|-----------|
| freezeFrameDTC | uint16_t | DTC que trigou |
| freezeFrameRPM | uint16_t | RPM at freeze |
| freezeFrameMAP | uint16_t | MAP (kPa) |
| freezeFrameTPS | uint8_t | TPS (%) |
| freezeFrameCLT | int8_t | Temp agua (C) |
| freezeFrameIAT | int8_t | Temp ar (C) |
| freezeFrameBaro | uint8_t | Baro (kPa) |
| freezeFrameVSS | uint8_t | Velocidade (km/h) |
| freezeFrameBattery | uint8_t | Bateria × 10 |
| freezeFrameO2 | uint8_t | AFR × 10 |
| freezeFrameAdvance | int8_t | Avanco (graus) |
| freezeFramePW | uint16_t | Pulsewidth (us) |
| freezeFrameLoad | uint8_t | Carga (%) |
| freezeFrameAFR | uint8_t | AFR target × 10 |
| freezeFrameFuel | uint8_t | Correcoes (%) |
| freezeFrameStatus1 | uint8_t | status1 flags |
| freezeFrameStatus2 | uint8_t | status2 flags |
| freezeFrameEngineFlags | uint8_t | engine flags |
| freezeFrameTimestamp | uint32_t | millis() |

---

## PARTE 7: EXEMPLO DE IMPLEMENTACAO DASHBOARD

### 7.1 Inicializacao CAN

```c
// Para STM32 com HAL
CAN_HandleTypeDef hcan;

void CAN_Init(void) {
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 6;      // 168MHz / 6 = 28MHz
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_6TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_7TQ;  // 28MHz / (1+6+7) = 2MHz / 4 = 500kbps
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = ENABLE;
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;
    HAL_CAN_Init(&hcan);

    // Filtro para aceitar todos os IDs relevantes
    CAN_FilterTypeDef filter;
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0x0000;
    filter.FilterIdLow = 0x0000;
    filter.FilterMaskIdHigh = 0x0000;
    filter.FilterMaskIdLow = 0x0000;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan, &filter);

    HAL_CAN_Start(&hcan);
}
```

### 7.2 Receber Dados Haltech

```c
typedef struct {
    uint16_t rpm;
    float map_kpa;
    float tps_pct;
    float fuel_pressure_psi;
    float oil_pressure_psi;
    float load_pct;
    float duty_cycle_pct;
    float advance_deg;
    uint16_t pw1_us, pw2_us, pw3_us, pw4_us;
    float lambda1, lambda2;
    float vss_kmh;
    uint8_t gear;
    float vvt1_deg, vvt2_deg;
    float battery_v;
    float boost_target_kpa;
    float baro_kpa;
    int16_t clt_c, iat_c, fuel_temp_c;
} ECU_Data_t;

ECU_Data_t ecu;

void CAN_ProcessHaltech(CAN_RxHeaderTypeDef* header, uint8_t* buf) {
    switch(header->StdId) {
        case 0x360:  // DATA1
            ecu.rpm = (buf[0] << 8) | buf[1];
            ecu.map_kpa = ((buf[2] << 8) | buf[3]) / 10.0f;
            ecu.tps_pct = ((buf[4] << 8) | buf[5]) / 5.0f;
            break;

        case 0x361:  // DATA2
            ecu.fuel_pressure_psi = (((buf[0] << 8) | buf[1]) - 1013) / 68.94f;
            ecu.oil_pressure_psi = (((buf[2] << 8) | buf[3]) - 1013) / 68.94f;
            ecu.load_pct = ((buf[4] << 8) | buf[5]) / 10.0f;
            break;

        case 0x362:  // DATA3
            ecu.duty_cycle_pct = ((buf[0] << 8) | buf[1]) / 10.0f;
            ecu.advance_deg = ((int16_t)((buf[4] << 8) | buf[5])) / 10.0f;
            break;

        case 0x364:  // PW
            ecu.pw1_us = (buf[0] << 8) | buf[1];
            ecu.pw2_us = (buf[2] << 8) | buf[3];
            ecu.pw3_us = (buf[4] << 8) | buf[5];
            ecu.pw4_us = (buf[6] << 8) | buf[7];
            break;

        case 0x368:  // LAMBDA
            ecu.lambda1 = ((buf[0] << 8) | buf[1]) / 1000.0f;
            ecu.lambda2 = ((buf[2] << 8) | buf[3]) / 1000.0f;
            break;

        case 0x370:  // VSS
            ecu.vss_kmh = ((buf[0] << 8) | buf[1]) / 10.0f;
            ecu.gear = buf[3];
            ecu.vvt1_deg = ((buf[4] << 8) | buf[5]) / 10.0f;
            ecu.vvt2_deg = ((buf[6] << 8) | buf[7]) / 10.0f;
            break;

        case 0x372:  // DATA4
            ecu.battery_v = buf[1] / 10.0f;
            ecu.boost_target_kpa = ((buf[4] << 8) | buf[5]) / 10.0f;
            ecu.baro_kpa = ((buf[6] << 8) | buf[7]) / 10.0f;
            break;

        case 0x3E0:  // DATA5
            ecu.clt_c = (((buf[0] << 8) | buf[1]) / 10) - 273;
            ecu.iat_c = (((buf[2] << 8) | buf[3]) / 10) - 273;
            ecu.fuel_temp_c = (((buf[4] << 8) | buf[5]) / 10) - 273;
            break;
    }
}
```

### 7.3 Send OBD-II Command

```c
void OBD_RequestPID(uint8_t mode, uint8_t pid) {
    CAN_TxHeaderTypeDef header;
    uint8_t data[8] = {0};
    uint32_t mailbox;

    header.StdId = 0x7DF;
    header.ExtId = 0;
    header.RTR = CAN_RTR_DATA;
    header.IDE = CAN_ID_STD;
    header.DLC = 8;
    header.TransmitGlobalTime = DISABLE;

    data[0] = 0x02;
    data[1] = mode;
    data[2] = pid;

    HAL_CAN_AddTxMessage(&hcan, &header, data, &mailbox);
}

// Uso:
// OBD_RequestPID(0x01, 0x0C);  // Requisitar RPM
// OBD_RequestPID(0x01, 0x05);  // Requisitar temp agua
// OBD_RequestPID(0x03, 0x00);  // Ler DTCs
```

---

## PARTE 8: SELECAO DE PROTOCOLO

No TunerStudio, configurar em **Settings → CAN Bus → Broadcast Protocol**:

| Valor | Protocolo | Uso Recomendado |
|-------|-----------|-----------------|
| 0 | OFF | Sem broadcast CAN |
| 1 | BMW | Paineis BMW E46/E39/E38 originais |
| 2 | VAG | Paineis VW/Audi originais |
| 3 | Haltech | Dashes Haltech IC-7/IC-10 ou **custom** |

**Recomendacao para Dashboard Custom:** Use **Haltech (3)** pois:
- Maior quantidade de dados (9 frames vs 3 do BMW)
- Frequencias maiores (50Hz para dados criticos)
- Big Endian facilita decode
- Protocolo bem documentado

---

*Documento gerado para projeto SCG-ECU 2.0 Dashboard - Janeiro 2026*
