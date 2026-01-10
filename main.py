import argparse

import cadquery as cq
from cadquery.selectors import AreaNthSelector


def create_positive_tool_from_negative_bin(body_wp: cq.Workplane) -> cq.Workplane:
    body_wp = body_wp.tag("original")

    # Check for singular solid in body
    if body_wp.solids().size() != 1:
        raise Exception("Body must have exactly one solid object")

    # How to create a negative body from tooltrace.ai
    # 1. Remove the original body's raised lip.
    #     A. Select the upper face that contains the tool cutout, and split body by that face
    #         i. +Z normal faces sorted by their elevation, top to bottom
    #             a. Thin upper lip
    #             b. Upper tool cutout   <- We need to split body by this face
    #             c. [... intermediate layers that may contain +Z normal faces ...]
    #             d. Lower tool cutout   <- We need this for step 2.
    # 2. Create a new body by extruding from split body's top face to lower tool cutout
    #     A. Due to the previous cut, there are multiple faces on the "top" side. Deconstruct the face into their wires
    #        then extrude from the maximal area-enclosing wire.
    # 3. Use the original to cut into the newly extruded body
    upper_tool_face_wp = body_wp.faces("+Z").faces(">Z[-2]")  # 2nd from the top
    lower_tool_face_wp = body_wp.faces("+Z").faces(">Z[0]")  # 1st from the bottom
    tool_z_depth = upper_tool_face_wp.workplane().plane.origin.z - lower_tool_face_wp.workplane().plane.origin.z

    body_wp = upper_tool_face_wp.workplane().split(keepBottom=True)
    body_wp = body_wp.faces(">Z[-1]").wires(AreaNthSelector(-1)).toPending().extrude(until=-tool_z_depth, combine=False)
    return body_wp.cut(body_wp.workplaneFromTagged("original"))


def main():
    # Argparser
    parser = argparse.ArgumentParser(description="Process input and output filenames.")
    parser.add_argument("input_file", type=str, help="Path to the input STEP file (.step or .stp)")
    parser.add_argument("output_file", type=str, help="Path to the output STL file (.stl)")

    # Parse arguments
    args = parser.parse_args()

    cq.exporters.export(
        create_positive_tool_from_negative_bin(cq.importers.importStep(args.input_file)),
        fname=args.output_file,
        exportType='STL'
    )


if __name__ == '__main__' or __name__ == '__cq_main__':
    main()
