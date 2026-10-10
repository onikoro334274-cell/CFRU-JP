"""Bounds shared by the ROM build and insertion entry points."""
MAX_ROM_SIZE = 0x2000000


def validate_payload(offset, size):
    if offset < 0 or size <= 0 or offset + size > MAX_ROM_SIZE:
        raise ValueError('Code must be nonempty and fit in the 32 MiB GBA ROM address range.')
