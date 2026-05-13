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
│   ├── gen_figures_hires.py    # Generate 300 DPI IEEE figures
│   ├── graphical_abstract.py  # Create graphical abstract
│   ├── Simulink_Documentation # Documentation for Simulink
│   ├── gen_figures_py_ Documentation  # Documentation for python code
│   └── Simulink_Simulation.m   # MATLAB simulation code
├── Docs/
│   ├── helios_artemis_manuscript.docx  # Research paper
│   └── cover_letter.docx       # Submission cover letter
├── FIG1-FIG9/                  # Individual figure scripts
├── figures_python/             # Generated Python figures
├── figures_simulink/           # Simulink-based figures
└── GRAPHICAL_ABSTRACT/         # Graphical abstract generation
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
