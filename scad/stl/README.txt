Kalos Dex STL files – ONE part per STL (arrange / duplicate them in the slicer yourself)
main/    current case parts. File name = colour_part_xQUANTITY  (any_ = colour doesn't matter)
         Print one colour at a time: red_*, then white_*, black_*, grey_*, any_*.
_build/  helper meshes used by tools/caps_split.py and tools/assembly_check.py – not for printing.
New fit-check sets go in test/<set_name>/ (one colour per set).
Not included: red_cap_x2, white_cap_rim_x2, black_cap_band_x2 and _build/ (made from the third-party prop STLs).
