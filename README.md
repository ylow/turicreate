Quick Links: [Installation](#supported-platforms) | [Documentation](#documentation)

[<img align="right" src="https://docs-assets.developer.apple.com/turicreate/turi-dog.svg" alt="Turi Create" width="100">](#)

# Fork

This is a fork of Turi Create to reestablish Mac compilation, clean up and
remove all neural network stuff (that are much harder to keep working).

There are many places for improvement:
 - support Python 3 stable ABI (PEP 384) (how?) so we don't need to compile
 a build for every other python version.
 - support fsspec in the filesystem abstraction
 - lambda workers can probably use PyInterpreter in the same process rather
 than subprocess.
 - Vectorized query execution

# Turi Create

Turi Create simplifies the development of custom machine learning models. You
don't have to be a machine learning expert to add recommendations, object
detection, image classification, image similarity or activity classification to
your app.

* **Easy-to-use:** Focus on tasks instead of algorithms
* **Visual:** Built-in, streaming visualizations to explore your data
* **Flexible:** Supports text, images, audio, video and sensor data
* **Fast and Scalable:** Work with large datasets on a single machine
* **Ready To Deploy:** Export models to Core ML for use in iOS, macOS, watchOS, and tvOS apps

With Turi Create, you can accomplish many common ML tasks:

| ML Task                 | Description                      |
|:------------------------:|:--------------------------------:|
| [Recommender](https://apple.github.io/turicreate/docs/userguide/recommender/)             | Personalize choices for users    |
| [Activity Classification](https://apple.github.io/turicreate/docs/userguide/activity_classifier/) | Detect an activity using sensors |
| [Image Similarity](https://apple.github.io/turicreate/docs/userguide/image_similarity/)        | Find similar images              |
| [Classifiers](https://apple.github.io/turicreate/docs/userguide/supervised-learning/classifier.html)             | Predict a label           |
| [Regression](https://apple.github.io/turicreate/docs/userguide/supervised-learning/regression.html)              | Predict numeric values           |
| [Clustering](https://apple.github.io/turicreate/docs/userguide/clustering/)              | Group similar datapoints together|
| [Text Classifier](https://apple.github.io/turicreate/docs/userguide/text_classifier/)         | Analyze sentiment of messages    |


Supported Platforms
-------------------

Turi Create supports:

* macOS 10.12+
* Linux (with glibc 2.10+)
* Windows 10 (via WSL)

System Requirements
-------------------

Turi Create requires:

* Python 2.7, 3.5, 3.6, 3.7, 3.8
* x86\_64 architecture
* At least 4 GB of RAM

Installation
------------

For detailed instructions for different varieties of Linux see [LINUX\_INSTALL.md](LINUX_INSTALL.md).
For common installation issues see [INSTALL\_ISSUES.md](INSTALL_ISSUES.md).

We recommend using virtualenv to use, install, or build Turi Create. 

```shell
pip install virtualenv
```

The method for installing *Turi Create* follows the
[standard python package installation steps](https://packaging.python.org/installing/).
To create and activate a Python virtual environment called `venv` follow these steps:

```shell
# Create a Python virtual environment
cd ~
virtualenv venv

# Activate your virtual environment
source ~/venv/bin/activate
```
Alternatively, if you are using [Anaconda](https://www.anaconda.com/what-is-anaconda/), you may use its virtual environment:
```shell
conda create -n virtual_environment_name anaconda
conda activate virtual_environment_name
```

To install `Turi Create` within your virtual environment:
```shell
(venv) pip install -U turicreate
```

Documentation
-------------

The package [User Guide](https://apple.github.io/turicreate/docs/userguide) and [API Docs](https://apple.github.io/turicreate/docs/api) contain
more details on how to use Turi Create.

Building From Source
---------------------

If you want to build Turi Create from source, see [BUILD.md](BUILD.md).

Contributing
------------

Prior to contributing, please review [CONTRIBUTING.md](CONTRIBUTING.md) and do
not provide any contributions unless you agree with the terms and conditions
set forth in [CONTRIBUTING.md](CONTRIBUTING.md).

We want the Turi Create community to be as welcoming and inclusive as possible, and have adopted a [Code of Conduct](CODE_OF_CONDUCT.md) that we expect all community members, including contributors, to read and observe.
