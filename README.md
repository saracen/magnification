# Magnification

Magnification is a C++ implemention of the paper [Eulerian Video Magnification for Revealing Subtle Changes in the World](http://people.csail.mit.edu/mrub/evm/).

This project uses much of the C++ implementation from [kgram007/Eulerian-Motion-Magnification](https://github.com/kgram007/Eulerian-Motion-Magnification), but removes some redundant steps and adds concurrency where possible to make processing a streaming video more performant.

## Requirements
* CMake
* OpenCV

## Building
### Linux
```
$ cmake .
$ make
```

### Windows
```
# Developer Command Prompt
$ cmake -DOpenCV_DIR=C:/OpenCV/build -G "Visual Studio 15 2017 Win64" .
$ devenv /build Release magnification.sln
```

## Running
```
# baby
$ ./magnification levels=6 alpha=10 lambda_c=16 cutoff_frequency_high=0.4 cutoff_frequency_low=0.05 http://people.csail.mit.edu/mrub/evm/video/baby.mp4

# baby2
$ ./magnification levels=6 alpha=150 lambda_c=600 cutoff_frequency_high=0.4 cutoff_frequency_low=0.04 http://people.csail.mit.edu/mrub/evm/video/baby2.mp4

# shadow
$ ./magnification alpha=5 lambda_c=48 cutoff_frequency_high=0.5 cutoff_frequency_low=0.16 chrom_attenuation=0 http://people.csail.mit.edu/mrub/evm/video/shadow.mp4

# camera
$ ./magnification levels=6 alpha=150 lambda_c=20 cutoff_frequency_high=0.5 cutoff_frequency_low=0.16 chrom_attenuation=0 http://people.csail.mit.edu/mrub/evm/video/camera.mp4

# wrist
$ ./magnification levels=6 alpha=10 lambda_c=16 cutoff_frequency_high=0.4 cutoff_frequency_low=0.05 http://people.csail.mit.edu/mrub/evm/video/wrist.mp4
```

OpenCV supports a range of video inputs, including `rtsp`, making this ideal for baby cameras etc. that support it:

```
$ ./magnification levels=6 alpha=10 lambda_c=16 cutoff_frequency_high=0.4 cutoff_frequency_low=0.05 rtsp://192.168.1.1/camera
```
