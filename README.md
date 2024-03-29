<div align="center">

# Patricia

[![License][license-badge]][license-link]  
[![GitHub release (latest by date)][release-badge]][release-link]
[![Commits since latest release][commits-badge]][commits-link]

</div>

A chess program made with the goal of objectifying chess style, and then training networks to excel at said style.

Code for filtering programs are found in the utils/ directory, and are indentified as follows:
- <b>position_filter_1:</b> Saves positions with general compensation - i.e. being down significantly in material yet still having at least a playable position.
- <b>position_filter_2:</b> Saves positions where one side's king is exposed and under attack by enemy pieces.
- <b>position_filter_3:</b> Saves positions where one side has a major space advantage.
- <b>position_filter_4:</b> Saves opposite side castling positions with lots of material on the board and one side attempting a pawn storm.
- <b>position_filter_5:</b> Saves positions where one side is significantly behind in development.
- <b>position_filter_6:</b> Saves positions where one side can't castle and has an open king. This is somewhat similar to 2 but doesn't take into account attackers and uses a different method for calculating defensive strength.
- <b>position_filter_7:</b> Saves positions where many pieces are able to move near, are near, or are pointing at the enemy king. This uses a simple but effective ray calculator instead of a full mobility calculation.
- <b>position_filter_8:</b> Saves positions where one side has an extremely powerful minor piece that can't be traded off easily. Many such positions result from exchange sacrifices and even if they don't the dominant piece is a long term advantage that often leads to dynamics down the line.


All filtering programs are run with the command `./[exe] [input.txt] [output.txt]`. You'll have to compile the particular program that you want to run yourself, or you can ask me for a binary for a specific filtering method.




[license-badge]: https://img.shields.io/github/license/Adam-Kulju/Patricia?style=for-the-badge
[release-badge]: https://img.shields.io/github/v/release/Adam-Kulju/Patricia?style=for-the-badge
[commits-badge]: https://img.shields.io/github/commits-since/Adam-Kulju/Patricia/latest?style=for-the-badge

[license-link]: https://github.com/Adam-Kulju/Patricia/blob/main/LICENSE
[release-link]: https://github.com/Adam-Kulju/Patricia/releases/latest
[commits-link]: https://github.com/Adam-Kulju/Patricia/commits/main
