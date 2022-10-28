# Foxholes Solver
## Synopsis
### The game
![](../foxholes/images/foz-puzzle.jpeg)

The Foxhole game is well described above. The original game has 5 holes in a row, with one hole "visited" (and fox caught) per day.

The goal is to capture the fox, no matter how wily, as long as the fox follows the rule to move every night.
 
### Playing the game

You can play the game [online](https://github.com/alfille/foxholes.github.io).

### Elaborations

Once you figure out how to solve the original game (hint, use the edges) the natural question is what happens if you change the rules?

#### Game harder
* **Length** Lengthen the line of holes
* **Width** Add more holes in width
* **Geometry** Change to a circle or triangle arrangement
* **Connections** Let the foxes jump 6 or 8 ways instead of only 4

#### Solution easier
* More hole **Visits** per day
* **Poison** the holes after a visit to make them uninhabitable for some number of days

All these choices are available in the solvers, and the [online game](https://github.com/alfille/foxholes.github.io).
  
### Solver
The purpose of this set of programs is to automatically find a winning strategy for a particular Foxhole game setup, or determine that it is not solvable. 

The solution can be downloaded to a file that can be loaded into the [web game](https://github.com/alfille/foxholes.github.io) to view.

## Game model

The game is making a number of moves (inspecting holes) to counteract the foxes moves in between.

### Game state


## Search strategies
## Pruning
## Implementation
### bit-maps
### 32-bit
### 64-bit
### depth-first
### GUI
## Use
### Installation and build
### Using
### License
## Visualization
