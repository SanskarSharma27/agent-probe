from dataclasses import dataclass
from typing import List, Optional
import json


@dataclass
class Record:
    id: int
    value: str
    processed: bool = False


def load_data(filepath):
    with open(filepath) as f:
        raw = json.load(f)
    return [Record(**item) for item in raw]


def transform(records):
    results = []
    for record in records:
        cleaned = clean_text(record.value)
        validated = validate_record(cleaned)
        results.append(validated)
    return results


def clean_text(text):
    return text.strip().lower()


def validate_record(text):
    if len(text) == 0:
        raise ValueError("Empty record")
    return text


def save_results(records, output_path):
    with open(output_path, "w") as f:
        json.dump(records, f)


def run_pipeline(input_path, output_path):
    data = load_data(input_path)
    transformed = transform(data)
    save_results(transformed, output_path)
    return len(transformed)
