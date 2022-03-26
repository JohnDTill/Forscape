import csv
from collections import namedtuple


def csv_to_list_of_tuples(csv_filepath, tuple_name="Entry", encoding="utf-8", delimiter=','):
    """
    Parse a CSV file to a list of named tuples.
    The first row of the CSV is assumed to contain headers,
    which are used to generate the named tuple properties.
    Tuple property names are converted to lowercase and spaces are replaced with underscores, '_'

    Every entry must be populated
    """
    with open(csv_filepath, encoding=encoding) as csv_file:
        reader = csv.reader(csv_file, delimiter=delimiter)
        headers = next(reader, None)
        headers = [header.replace(' ', '_') for header in headers]
        Entry = namedtuple(tuple_name, (' '.join(headers)).lower())
        return [Entry(*row) for row in reader]


def csv_headers(csv_filepath, encoding="utf-8", delimiter=','):
    """
    Parse a CSV file to a list of named tuples.
    The first row of the CSV is assumed to contain headers,
    which are used to generate the named tuple properties.
    Tuple property names are converted to lowercase and spaces are replaced with underscores, '_'

    Every entry must be populated
    """
    with open(csv_filepath, encoding=encoding) as csv_file:
        reader = csv.reader(csv_file, delimiter=delimiter)
        headers = next(reader, None)
        return headers
