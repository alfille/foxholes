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

**State -> Move -> State -> ...**

where **State** is the state of all the foxholes and **Move** is all the hole inspections on a given day

### Game state

A **Game State** can be represented like this:

![Game state](images/10.png)

This shows potential foxes in 2 of the holes.

For simplicity we can call this Game State:

**0 1 0 1 0** where foxes 1 and empty holes are 0. This is a binary number corresponding to 10 in decimal.

So this is Game State **S10**
___

The Goal of the game is to go from Game State:
![](images/31.png)

to Game State:
![](images/0.png)

Or **1 1 1 1 1** -> **0 0 0 0 0**

Or S31 -> S0
___

Actually S0 is shown as:
![](images/happy.png)

in the game to flag this as a win!
___
Note that there are `2^holes` number of Game States (`2^5 = 32	 in this case)

### Moves

Between each Game State, you can inspect up to **visits** number of holes. For example:

![](images/M8.png)

This could also be represented in binary:

**0 1 0 0 0** or **M8**
___

The number of possible moves is the
*binomial coefficient*:

`B( holes, visits )` = holes!/(visits!*(holes-visits)!)

### Game representation

The game start:

State: ![](images/31.png)

Move: ![](images/M8.png)

to State ![](images/15.png)

is:  **S31 -> M8 -> S15**

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
