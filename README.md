Labrstim
--------

Real-time laser or electric brain stimulation for use with a Raspberry Pi,
Intan RHD2000 board and the custom-made [GALDUR hat](https://github.com/bothlab/galdur)
for the Raspberry Pi.

The software is comprised of a server component, running on the Raspberry Pi, that
communicates with the GALDUR board to sample analog data exported from electrophysiological
recordings. The analog data in our tests came from an [Intan RHD USB Interface Board](ttps://intantech.com/RHD_USB_interface_board.html).

The software currently has the following features:

* SWR Stimulation: Detect & disrupt sharp-wave ripples online.
  To do that, this project uses code used in Van De Ven et al. "Hippocampal Offline Reactivation Consolidates Recently Formed Cell Assembly Patterns during Sharp Wave-Ripples",
  [Neuron 92, 968–974 (2016)](https://doi.org/10.1016/j.neuron.2016.10.020). The original code is
  available on GitHub at [kevin-allen/laser_stimulation](https://github.com/kevin-allen/laser_stimulation)
* Theta Stimulation: Stimulate on specified theta phase
* Spikerate Stimulation: Stimulate when a specific spike frequency was detected
* Train Stimulation: Perform random or train stimulation, without reading input signal data

To use this software, flash the image provided onto an SDCard (or built it from scratch using the `os-image/build.sh` script on a Linux system)
and add it to a Raspberry Pi 4+.
Assemble and connect a [GALDUR board](https://github.com/bothlab/galdur) and connect it to the Raspberry Pi. The USB serial output of the
GALDUR board is connected to your computer.

You can then use the provided client software to give the GALDUR+RPi detection/stim-trigger box commands.
The box can also be used from the [Syntalos](https://syntalos.org/) DAQ system and is integrated well with it.
Syntalos implements the same client component that also exists in this repository as standalone application.
