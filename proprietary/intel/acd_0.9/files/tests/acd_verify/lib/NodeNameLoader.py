# This file is for internal use: place this file to tests/lib/ directory and make sure you have installed pandas and 

import importlib

def apply_node_name_from_excel(headers, tinfo, file_path):
    try:
        module = importlib.import_module(f'lib.{file_path}')
    except ImportError:
        print(f"Failed to import {file_path}")
        return

    result_dict = getattr(module, 'node_data', None)

    if result_dict is None:
        return

    headers.insert(0, "Node Name")     
    tinfo["Node Name"] = [result_dict.get(sec.split("-")[1], 'decode failed') if "-" in sec and len(sec.split("-")) > 1 else '' for sec in tinfo["Section"]]
    tinfo["Node Name"] = ['.'.join(elem.split('.')[2:]) if elem.count('.') >= 2 else elem for elem in tinfo["Node Name"]]

