"""
Marila Programming Language - AST Node Definitions
"""

class ASTNode:
    pass

class ProgramNode(ASTNode):
    def __init__(self, file_type: str, statements: list, render_target: str = None):
        self.file_type = file_type # 'video', 'document', or 'image'
        self.statements = statements
        self.render_target = render_target

class DocumentNode(ASTNode):
    def __init__(self, node_id: str, text: str, subtitle: str = None):
        self.node_id = node_id
        self.text = text
        self.subtitle = subtitle

class MorphNode(ASTNode):
    def __init__(self, from_shape: dict, to_shape: dict, duration: str = "3s"):
        self.from_shape = from_shape
        self.to_shape = to_shape
        self.duration = duration

class PlotNode(ASTNode):
    def __init__(self, expression: str, animate_trace: bool = True):
        self.expression = expression
        self.animate_trace = animate_trace

class PointNode(ASTNode):
    def __init__(self, point_id: str, x: float, y: float, label: str = None):
        self.point_id = point_id
        self.x = x
        self.y = y
        self.label = label or point_id

class LineNode(ASTNode):
    def __init__(self, from_id: str, to_id: str):
        self.from_id = from_id
        self.to_id = to_id

class TriangleNode(ASTNode):
    def __init__(self, point_ids: list, color: str = "amber", show_angles: bool = True):
        self.point_ids = point_ids
        self.color = color
        self.show_angles = show_angles

class CircleNode(ASTNode):
    def __init__(self, center_id: str, radius: float = 50.0, dash: bool = False):
        self.center_id = center_id
        self.radius = radius
        self.dash = dash
