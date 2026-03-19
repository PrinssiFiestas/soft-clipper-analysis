# Soft Clipper Analysis

This repository contains experiments for [our study about clipping softness](https://lorenzofiestas.dev/articles/quantifying-clipping-softness/). Implementation details for the main experiment to find the softest clipper can also be found in that study. 

## Platform Support and Dependencies

Only Linux is tested. On Windows, WSL can be used of course. You will need `gcc` and `make` for all experiments. For the main experiment, you will also need to have `libX11`, `libGLX`, and `libGL` installed. 

## Running Experiments

Run 

```bash
make
```

to see available experiments. To run an experiment in any given `BASE`, run 

```bash
make <experiment> BASE=<base> [options]
```

The main experiment (described in the study) can be run by running 

```bash
make smoothest BASE=<base> [HFC=<1|0>]
```

If `HFC=1` is passed, then the experiment uses WTHD hardness definition. Otherwise, second derivative based hardness definition is used instead. After running the main experiment, additional information about the results can be obtained by running 

```bash
make analyze BASE=<base> [HFC=<1|0>]
```

## TODO

Most experiments and code are not well documented (sorry!). 