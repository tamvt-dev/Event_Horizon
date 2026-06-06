## Training Data Sources

Event Horizon is designed to train on custom text corpora and Q&A datasets.

Current experiments include:

* General-purpose Q&A datasets
* Synthetic question-answer corpora
* Technical documentation
* Selected Linux kernel documentation for systems programming concepts

Linux kernel documentation is used as an optional knowledge source for generating technical Q&A examples and vocabulary expansion. The engine is not trained on the complete Linux kernel source tree, and Linux kernel source code is not embedded in the runtime by default.

When Linux-derived documentation examples are used, experiments should document the source version, preprocessing steps, and any generated artifacts.