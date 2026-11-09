"""Compatibility adapter to the Canterbury submodule's canonical geometry review.

The compiler-owned mirror is retained for existing content provenance; reject
it if it differs from the canonical submodule review rather than silently drift.
"""
import copy, importlib.util, json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CORRECTIONS='content/building-placement-corrections.json'
_spec=importlib.util.spec_from_file_location('canterbury_canonical_placements',ROOT/'canterbury/simulation-map/building_placements.py')
_core=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_core)
SOURCE_FILES=_core.SOURCE_FILES

def canonical_document(document):
    result=copy.deepcopy(document)
    if 'structure_sources' in result:
        result['structure_sources']={p.removeprefix('canterbury/'):v for p,v in result['structure_sources'].items()}
    return result

def load_corrections():
    document=json.loads((ROOT/CORRECTIONS).read_text())
    if canonical_document(document)!=_core.load_corrections():
        raise ValueError('Compiler-owned building review differs from canonical Canterbury submodule review')
    return document

def corrected_objects(objects,document=None):
    return _core.corrected_objects(objects,canonical_document(load_corrections() if document is None else document))
