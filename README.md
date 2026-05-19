# Helios-Artemis

A photovoltaic (PV) system research project implementing advanced Maximum Power Point Tracking (MPPT) algorithms for solar energy optimization.

## Overview

This project presents a comprehensive study on solar PV system optimization through:
- Advanced MPPT algorithms (Perturb & Observe, Incremental Conductance, etc.)
- LSTM-based irradiance prediction
- Real-time simulation and validation
- 9 publication-quality IEEE figures

## Project Structure

```
artemis-helios/
├── Code/
│   ├── MatLab/                     # MATLAB simulation & analysis scripts
│   │   ├── Artemis_Helios_Simulation.m
│   │   ├── HA_MPPT_v3.slx          # Simulink model
│   │   ├── Simulink_Simulation.m
│   │   ├── ha_artemis_v3.m         # Main Artemis controller
│   │   ├── ha_battery_v3.m         # Battery model
│   │   ├── ha_buck_v3.m            # Buck converter
│   │   ├── ha_irr_v3.m             # Irradiance processing
│   │   ├── ha_lstm_v3.m            # LSTM prediction
│   │   ├── ha_pv_v3.m              # PV panel model
│   │   ├── dailysummary.fig
│   │   └── energysummary.fig
│   ├── Python/                     # Python scripts
│   │   ├── 01_irradiance_generator.py
│   │   ├── 02_lstm_training.py
│   │   ├── 03_mppt_controllers.py
│   │   ├── gen_figures_hires.py
│   │   └── graphical_abstract.py
│   ├── documentation/              # Project documentation
│   ├── figures/                    # High-resolution IEEE figures
│   └── __pycache__/
├── Docs/
│   ├── Presentation/               # PowerPoint presentations
│   ├── Study_materials/            # Study guides & primers
│   └── Upload_package/             # MDPI submission package
├── Figures/
│   ├── figures_python/             # Python-generated figures (FIG1-FIG9)
│   │   ├── FIG1/ ... FIG9/         # Individual figure scripts & outputs
│   ├── GRAPHICAL_ABSTRACT/         # Graphical abstract generation
│   └── MATLAB Figures/             # MATLAB exported figures
├── Tables/                         # CSV data tables
│   ├── helios_artemis_irradiance_representative_days.csv
│   ├── helios_artemis_year1_training_daily_summary.csv
│   └── helios_artemis_year2_test_daily_summary.csv
├── prototype/
│   ├── Code/                       # Firmware (STM32 & ESP32-S3)
│   ├── Guide/                      # Schematics, PCB, layout guide
│   └── schematics/                 # KiCad design files
└── README.md
```

## Figures

1. **FIG1** - System Architecture
2. **FIG2** - Irradiance Data Analysis
3. **FIG3** - IV Curves
4. **FIG4** - LSTM Prediction Model
5. **FIG5** - Simulation Results
6. **FIG6** - Algorithm Comparison
7. **FIG7** - PO Convergence Analysis
8. **FIG8** - Cost Analysis
9. **FIG9** - Experimental Validation

## Requirements

Each figure directory contains its own `requirements.txt`. General requirements:
- Python 3.x
- NumPy, Matplotlib, SciPy

## Running

Generate all figures:
```bash
cd Code && python gen_figures_hires.py
```

Generate specific figure:
```bash
cd FIG1 && python fig1.py
```

## Author

**Hussain Touhid Siddiquee**  
Department of Electrical & Electronic Engineering  
Leading University, Sylhet

## License

MIT License
