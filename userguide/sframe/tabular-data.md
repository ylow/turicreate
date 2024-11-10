# Working with Tabular Data

It's quite common that when you first get your hands on a dataset, it
will be in a format that resembles a table. Tables are a straightforward
format to use when cleaning data in preparation for more complicated
data analysis, and the
[XFrame](https://apple.github.io/turicreate/docs/api/generated/turicreate.XFrame.html)
is the tabular data structure included with Turi Create. The XFrame is
designed to scale to datasets much larger than will fit in memory.

We will introduce the basics of the XFrame in the following chapters:

* [XFrames](xframe-intro.md) focuses on creating an XFrame
  from existing data in CSV format and how to persist an XFrame.

* The XFrame supports a large number of common data manipulation
  operations and we will review a number of common ones in the chapter
[Data Manipulation](data-manipulation.md).

* The chapter about [SQL
  databases](../data_formats_and_sources/sql_integration.md) explains
how to interface with relational data sources through Python DBAPI2 or
ODBC.
