"""Unit tests for dependency topological sort."""

from __future__ import annotations

import unittest

from ueremcp_protocol.dependency_order import DependencyError, topological_sort


class TopoTests(unittest.TestCase):
    def test_linear(self):
        nodes = [
            {"id": "a", "depends_on": []},
            {"id": "b", "depends_on": ["a"]},
            {"id": "c", "depends_on": ["b"]},
        ]
        self.assertEqual(topological_sort(nodes), ["a", "b", "c"])

    def test_diamond_preserves_input_order_among_ready(self):
        nodes = [
            {"id": "material", "depends_on": []},
            {"id": "trail", "depends_on": []},
            {"id": "fx", "depends_on": ["material", "trail"]},
            {"id": "ability", "depends_on": ["fx"]},
        ]
        self.assertEqual(
            topological_sort(nodes), ["material", "trail", "fx", "ability"]
        )

    def test_cycle_rejected(self):
        nodes = [
            {"id": "a", "depends_on": ["b"]},
            {"id": "b", "depends_on": ["a"]},
        ]
        with self.assertRaises(DependencyError) as ctx:
            topological_sort(nodes)
        self.assertIn("cycle", str(ctx.exception).lower())

    def test_missing_dep(self):
        with self.assertRaises(DependencyError):
            topological_sort([{"id": "a", "depends_on": ["missing"]}])

    def test_duplicate_id(self):
        with self.assertRaises(DependencyError):
            topological_sort(
                [{"id": "a", "depends_on": []}, {"id": "a", "depends_on": []}]
            )


if __name__ == "__main__":
    unittest.main()
