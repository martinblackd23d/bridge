# Run

```gcc -o bridge bridge.c -Wall -Werror```

1. User input:

```./bridge```

then follow prompts

2. Command line args:

```./bridge <vehicle count> <probability of northbound> <delay> { <vehicle count> <probability of northbound> <delay> ... }```

eg. ```./bridge 10 0.5 5 10 0.8 0``` 

3. File

```./bridge <schedule file>```

eg. ```./bridge schedule.txt```

File format:

```
<number of groups>

<vehicle count> <probability of northbound> <delay>

<vehicle count> <probability of northbound> <delay>

...
```

## Notes

- vehicle count: the number of vehicles arriving at the same time

- probability of northbound: the probability of a vehicle to be generated as northbound, otherwise it's southbound

- delay: time to wait in between groups

- a delay is still required for the last group, although it's value does not matter

## Known issues:

None
