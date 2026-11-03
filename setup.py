from pathlib import Path

from setuptools import find_packages, setup


setup(
    name="marila",
    version="1.1.0",
    description="A hybrid mathematical programming language for computation, documents, geometry, and animation.",
    long_description=Path("README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    packages=find_packages(),
    python_requires=">=3.9",
    entry_points={"console_scripts": ["marila=marila_cli:main"]},
)
