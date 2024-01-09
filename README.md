# Patricia

A chess program made with the goal of objectifying chess style, and then training networks to excel at said style.

Code for filtering programs are found in the utils/ directory, and are indentified as follows:
- <b>position_filter_1:</b> Saves positions with general compensation - i.e. being down significantly in material yet still having at least a playable position.
- <b>position_filter_2:</b> Saves positions where one side's king is exposed and under attack.
- <b>position_filter_3:</b> Saves positions where one side has a major space advantage.

All filtering programs are run with the command `./[exe] [input.txt] [output.txt]`. You'll have to compile the particular program that you want to run yourself, or you can ask me for a binary for a specific filtering method.
