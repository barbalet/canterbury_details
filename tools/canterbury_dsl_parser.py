"""Bounded lexer and parser shared by Canterbury authoring tools."""
import bisect
import json
import math
import re

MAX_BYTES = 8 * 1024 * 1024
MAX_DEPTH = 64
MAX_RECORDS = 65536
IDENTIFIER = re.compile(r'[A-Za-z_][A-Za-z0-9_.-]{0,127}(?![A-Za-z0-9_.-])')
TRIVIA = re.compile(r'(?:\s+|#[^\n]*)*')
NUMBER = re.compile(r'-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?')
LITERALS = {'true': True, 'false': False, 'null': None, 'unset': None}
DECODER = json.JSONDecoder()


class DraftError(ValueError):
    pass


class Records(dict):
    """Values and source locations, kept separately for canonical compilation."""

    def __init__(self, source, text):
        super().__init__()
        self.source = source
        self.lines = [match.start() for match in re.finditer('\n', text)]
        self.locations = {}

    def error(self, key, message, field=None):
        position = self.locations.get((*key, field), self.locations.get(key, 0))
        line = bisect.bisect_left(self.lines, position)
        previous = self.lines[line - 1] if line else -1
        return DraftError(f'{self.source}:{line + 1}:{position - previous}: {message}')


class Parser:
    def __init__(self, text, source):
        if len(text.encode('utf-8')) > MAX_BYTES:
            raise DraftError(f'{source}: file exceeds 8 MiB')
        self.text = text
        self.position = 0
        self.records = Records(source, text)

    def error(self, message):
        key = ('<syntax>', '')
        self.records.locations[key] = self.position
        return self.records.error(key, message)

    def peek(self):
        self.position = TRIVIA.match(self.text, self.position).end()
        return self.text[self.position:self.position + 1]

    def take(self, symbol):
        if self.peek() != symbol:
            raise self.error(f'expected {symbol!r}')
        self.position += 1

    def identifier(self):
        self.peek()
        match = IDENTIFIER.match(self.text, self.position)
        if match is None:
            raise self.error('expected identifier of at most 128 characters')
        self.position = match.end()
        return match[0]

    def value(self, depth=0):
        if depth > MAX_DEPTH:
            raise self.error('value nesting exceeds 64')
        char = self.peek()
        if char == '[':
            self.take('[')
            values = []
            if self.peek() != ']':
                values.append(self.value(depth + 1))
                while self.peek() == ',':
                    self.take(',')
                    values.append(self.value(depth + 1))
            self.take(']')
            return values
        if char == '"':
            try:
                value, self.position = DECODER.raw_decode(self.text, self.position)
                value.encode('utf-8')
            except (ValueError, UnicodeError) as error:
                raise self.error('invalid Unicode string') from error
            return value
        match = NUMBER.match(self.text, self.position)
        if match:
            token = match[0]
            if len(token) > 128:
                raise self.error('numeric literal too long')
            value = json.loads(token)
            if type(value) is int and not -(2**63) <= value < 2**63:
                raise self.error('integer outside signed 64-bit range')
            if type(value) is float and not math.isfinite(value):
                raise self.error('non-finite number')
            self.position = match.end()
            return value
        literal = self.identifier()
        if literal not in LITERALS:
            raise self.error(f'unsupported value {literal!r}')
        return LITERALS[literal]

    def parse(self):
        while self.peek():
            start = self.position
            key = self.identifier(), self.identifier()
            if key in self.records:
                raise self.error(f'duplicate declaration {" ".join(key)}')
            if len(self.records) >= MAX_RECORDS:
                raise self.error('declaration budget exceeded')
            self.records.locations[key] = start
            self.take('{')
            fields = {}
            while self.peek() != '}':
                start = self.position
                field = self.identifier()
                if field in fields:
                    raise self.error(f'duplicate field {field}')
                self.records.locations[(*key, field)] = start
                fields[field] = self.value()
                self.take(';')
            self.take('}')
            self.records[key] = fields
        return self.records


def parse(text, source='<scenario>'):
    return Parser(text, source).parse()
