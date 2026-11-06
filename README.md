# Kill Switch Analysis

Does "just turn it off" actually turn it off?

This tool checks whether a program, and every process it started, really stops when you kill it. It reads the program's machine code to predict what will happen, then runs it and tries to kill it four different ways.

```
O = everything stopped, X = something kept running

orphan: forks
polite X  kill -9 X  group O  tree O

sneaky: catches SIGTERM, forks, setsid (raw)
polite X  kill -9 X  group X  tree O
```

## Motivation

A lot of AI safety plans end with "if it goes wrong, we switch it off". For an AI agent that usually means killing one process. However, to achieve their goal, agents take a lot of action on their own, they start shells, dev servers and background jobs, and those can keep running after the agent is gone.

Models can also resist on purpose: Palisade Research found that OpenAI's o3 sabotaged its own shutdown script in 79 of 100 runs (https://www.theregister.com/2025/05/29/openai_model_modifies_shutdown_script/[The Register, 2025]).
Therefore, considering the above, this project looks one layer lower, at whether the off switch itself works to begin with.

Made for the Safe AI Netherlands technical track, linked to chapters 1.5, 3.2 and 4.4 of https://www.aisafetybook.com[AI Safety, Ethics and Society].

## Methdology

The program requires Linux on x86-64. On a Mac, it is advisable to use Docker:

```
docker run -it --rm --platform linux/amd64 -v "$PWD":/w -w /w ubuntu:24.04 bash
apt-get update && apt-get install -y gcc make binutils
make audit
```

[cols="1,3"]
|===
|`make audit` |builds and audits all demo programs
|`./audit demo/sneaky` |audit one program
|`make test` |check the output still matches `expected.txt`
|`make clean` |remove everything that was built
|===


## Analysis Approaches

Everything is in `audit.c`.

*Static Analysis* `objdump` turns the program into assembly. The tool looks for calls that change how a program dies: catching SIGTERM, `fork`, `setsid` and `prctl(PR_SET_PDEATHSIG)`. It also decodes raw `syscall` instructions, so a program that skips the C library can't hide.

*Dynamic Analysis* The tool starts the program, waits a second and kills it:

[cols="1,3"]
|===
|polite |SIGTERM to the main process
|kill -9 |SIGKILL to the main process
|group |SIGKILL to its process group
|tree |SIGKILL to every process below it
|===

Then it checks `/proc` for anything still alive. It registers as a subreaper first, so orphaned children are handed to it and can still be counted.

## Demo Experiment

[cols="1,3"]
|===
|`obedient` |does nothing special, dies with any kill
|`stubborn` |catches SIGTERM and ignores it
|`orphan` |starts a child that outlives it
|`runaway` |the child also leaves the process group with `setsid`
|`leashed` |the child dies with its parent (`PR_SET_PDEATHSIG`), the fix
|`sneaky` |written in assembly with no imports, does all of the above
|`devserver.sh` |starts a background job, like a coding agent
|===

## Findings

* `kill -9` on the main process left something running in 4 of 7 programs.
* Only killing the whole tree stopped everything every time.
* Reading the machine code predicted every result before anything ran, even for `sneaky`, which imports nothing.

A better off switch kills the whole tree (a cgroup with `cgroup.kill`), leashes children with `PR_SET_PDEATHSIG`, and checks afterwards that nothing is left.

## Limitations

* Only tested on small demo programs so far, not real agents.
* The static scan expects unoptimised code. Optimised or stripped binaries can hide arguments.
* The tree kill misses processes started after its snapshot. However, a cgroup would fix that.
* It tests the process layer. It does NOT test a model rewriting its own shutdown script.
