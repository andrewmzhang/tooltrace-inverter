# tooltrace-inverter

A Python script that converts a Gridfinity bin with a tool negative (generated on [Tooltrace](https://tooltrace.ai))
into a tool model positive.

The resulting tool model can then be imported into [Gridfinity Generator](https://gridfinitygenerator.com) to create
bins with magnet support, a feature that Tooltrace does not currently provide.

---

## Workflow

1. Generate a Gridfinity bin with a tool negative using [Tooltrace](https://tooltrace.ai)
2. Download the Tooltrace bin as a STEP file
3. Run this script to invert the tool negative into a tool positive model
   1. `python main.py gridfinity_bin.step tool_model.stl`
4. Open [Gridfinity Generator](https://gridfinitygenerator.com) -> Cutout -> General -> Browse -> `tool_model.stl`

---

## License

Copyright © 2026 Andrew M. Zhang  
All rights reserved.
