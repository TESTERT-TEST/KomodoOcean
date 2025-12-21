# Checkpoints Tests

This directory contains tests for verifying automatic checkpoint functionality in KomodoOcean.

## Requirements

- Python 3.6 or higher
- Python virtual environment (recommended)

## Installation

### 1. Create Virtual Environment

```bash
cd /media/decker/data1tb/auto-checkpoints/KomodoOcean/qa/checkpoints-tests
python3 -m venv venv
```

### 2. Activate Virtual Environment

**Linux/macOS:**
```bash
source venv/bin/activate
```

**Windows:**
```bash
venv\Scripts\activate
```

### 3. Install Dependencies

```bash
pip install --upgrade pip
pip install -r requirements.txt
```

## Running Tests

After activating the virtual environment and installing dependencies, you can run the tests:

```bash
python3 ./try-fork-before-checkpoint.py
```

## Quick Setup (One Command)

You can use the `setup_venv.sh` script for automatic setup:

```bash
bash ./setup_venv.sh
```

This will create the virtual environment, install dependencies, and activate it.

## Deactivating Virtual Environment

When you're done working, deactivate the virtual environment:

```bash
deactivate
```

## Dependencies

- `simplejson` - JSON library (faster alternative to the standard json module)

## Notes

- Make sure you are in the `qa/checkpoints-tests` directory before running tests
- The virtual environment is created locally in the `venv/` directory and should not be added to version control

## Block diagram

```mermaid
graph TD
    subgraph node0["Node 0 (blocks 120-133)"]
        direction TB
        B0_120["Block 120<br/>01e7...eeab"]:::block
        CP0_120["CP→Block 110<br/>055a...cebd"]:::checkpoint
        B0_120 --> CP0_120
        B0_121["Block 121<br/>03f4...c9f7"]:::block
        B0_120 --> B0_121
        CP0_121["CP→Block 111<br/>026c...1265"]:::checkpoint
        B0_121 --> CP0_121
        B0_122["Block 122<br/>0144...468a"]:::block
        B0_121 --> B0_122
        CP0_122["CP→Block 112<br/>022b...7c17"]:::checkpoint
        B0_122 --> CP0_122
        B0_123["Block 123<br/>01e3...2d53"]:::block
        B0_122 --> B0_123
        CP0_123["CP→Block 113<br/>0513...6613"]:::checkpoint
        B0_123 --> CP0_123
        B0_124["Block 124<br/>0080...cf87"]:::block
        B0_123 --> B0_124
        CP0_124["CP→Block 114<br/>0236...5ba8"]:::checkpoint
        B0_124 --> CP0_124
        B0_125["Block 125<br/>02cc...fe50"]:::block
        B0_124 --> B0_125
        CP0_125["CP→Block 115<br/>0147...d4cc"]:::checkpoint
        B0_125 --> CP0_125
        B0_126["Block 126<br/>007b...e3f9"]:::block
        B0_125 --> B0_126
        CP0_126["CP→Block 116<br/>0491...fc00"]:::checkpoint
        B0_126 --> CP0_126
        B0_127["Block 127<br/>030a...72cf"]:::block
        B0_126 --> B0_127
        CP0_127["CP→Block 117<br/>017a...1249"]:::checkpoint
        B0_127 --> CP0_127
        B0_128["Block 128<br/>006e...a51f"]:::block
        B0_127 --> B0_128
        CP0_128["CP→Block 118<br/>04f3...08c2"]:::checkpoint
        B0_128 --> CP0_128
        B0_129["Block 129<br/>01e1...4600"]:::block
        B0_128 --> B0_129
        CP0_129["CP→Block 119<br/>02c1...b3e9"]:::checkpoint
        B0_129 --> CP0_129
        B0_130["Block 130<br/>033e...c26d"]:::block
        B0_129 --> B0_130
        CP0_130["CP→Block 120<br/>01e7...eeab"]:::checkpoint
        B0_130 --> CP0_130
        CP0_130 -.-> B0_120
        B0_131["Block 131<br/>0237...4451"]:::block
        B0_130 --> B0_131
        CP0_131["CP→Block 121<br/>03f4...c9f7"]:::checkpoint
        B0_131 --> CP0_131
        CP0_131 -.-> B0_121
        B0_132["Block 132<br/>0255...2fa0"]:::block
        B0_131 --> B0_132
        CP0_132["CP→Block 122<br/>0144...468a"]:::checkpoint
        B0_132 --> CP0_132
        CP0_132 -.-> B0_122
        B0_133["Block 133<br/>016b...ed3f"]:::block
        B0_132 --> B0_133
        CP0_133["CP→Block 123<br/>01e3...2d53"]:::checkpoint
        B0_133 --> CP0_133
        CP0_133 -.-> B0_123
    end
    
    subgraph node1["Node 1 (blocks 120-137)"]
        direction TB
        B1_120["Block 120<br/>01e7...eeab"]:::block
        B1_121["Block 121<br/>03f4...c9f7"]:::block
        B1_120 --> B1_121
        B1_122["Block 122<br/>0144...468a"]:::block
        B1_121 --> B1_122
        B1_123["Block 123<br/>00b8...cb67"]:::block
        B1_122 --> B1_123
        B1_124["Block 124<br/>03b5...6d25"]:::block
        B1_123 --> B1_124
        B1_125["Block 125<br/>001c...2143"]:::block
        B1_124 --> B1_125
        B1_126["Block 126<br/>02c7...fd69"]:::block
        B1_125 --> B1_126
        B1_127["Block 127<br/>0137...3d67"]:::block
        B1_126 --> B1_127
        B1_128["Block 128<br/>02cb...fa2a"]:::block
        B1_127 --> B1_128
        B1_129["Block 129<br/>063b...753c"]:::block
        B1_128 --> B1_129
        B1_130["Block 130<br/>056b...619b"]:::block
        B1_129 --> B1_130
        B1_131["Block 131<br/>04db...ff94"]:::block
        B1_130 --> B1_131
        B1_132["Block 132<br/>055e...ffcf"]:::block
        B1_131 --> B1_132
        B1_133["Block 133<br/>0375...8141"]:::block
        B1_132 --> B1_133
        B1_134["Block 134<br/>05bd...c1b7"]:::block
        B1_133 --> B1_134
        B1_135["Block 135<br/>0351...d972"]:::block
        B1_134 --> B1_135
        B1_136["Block 136<br/>0650...aa6d"]:::block
        B1_135 --> B1_136
        B1_137["Block 137<br/>0541...aca2"]:::block
        B1_136 --> B1_137

    B0_122 -.->|Common history| B1_122
    end
    
    classDef block fill:#4A90E2,stroke:#2E5C8A,stroke-width:2px,color:#fff
    classDef checkpoint fill:#7ED321,stroke:#5FA319,stroke-width:2px,color:#000
```
