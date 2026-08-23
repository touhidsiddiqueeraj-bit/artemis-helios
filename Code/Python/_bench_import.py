"""Internal imports for the sensitivity heatmap (avoids the dotted filename).

Registers 03_mppt_controllers.py under the module name 'mppt_controllers'
matching what 04_transient_benchmark.py uses, so both share ONE class object
and `isinstance(ctrl, LSTMAssistedPaO)` matches inside run_controller().
"""
import os
import sys
from importlib.util import spec_from_file_location, module_from_spec

_HERE = os.path.dirname(os.path.abspath(__file__))


def _load(name, fname):
    if name in sys.modules:
        return sys.modules[name]
    spec = spec_from_file_location(name, os.path.join(_HERE, fname))
    if spec is None or spec.loader is None:
        raise ImportError(f'cannot load {fname}')
    mod = module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


_mc = _load('mppt_controllers', '03_mppt_controllers.py')
_tb = _load('04_transient_benchmark', '04_transient_benchmark.py')

LSTMAssistedPaO = _mc.LSTMAssistedPaO
pv_power = _tb.pv_power          # true-model port used by the benchmark harness
stochastic_day = _tb.stochastic_day
mpp_series = _tb.mpp_series
compute_tracking_efficiency = _tb.compute_tracking_efficiency
