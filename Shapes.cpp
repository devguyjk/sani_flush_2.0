#include "Shapes.h"
#include "draw_functions.h" // For writeLog
// Shape implementation
Shape::Shape(String _name) : name(_name) {}
void Shape::printDetails(bool shouldPrint) {
  if (!shouldPrint) return;
  writeLog("Shape: Name = %s", name.c_str());
}

// Rectangle implementation
Rectangle::Rectangle(String _name, int _x, int _y, int _width, int _height)
  : Shape(_name), x(_x), y(_y), width(_width), height(_height) {}

bool Rectangle::isTouched(int16_t touchX, int16_t touchY) {
  return (touchX >= x && touchX <= (x + width) && touchY >= y && touchY <= (y + height));
}

void Rectangle::printDetails(bool shouldPrint) {
  if (!shouldPrint) return;
  Shape::printDetails(shouldPrint);
  writeLog("Rectangle: X=%d, Y=%d, Width=%d, Height=%d", x, y, width, height);
}

// Ellipse implementation
Ellipse::Ellipse(String _name, int _centerX, int _centerY, int _a, int _b)
  : Shape(_name), centerX(_centerX), centerY(_centerY), a(_a), b(_b) {}

bool Ellipse::isTouched(int16_t touchX, int16_t touchY) {
  // Ellipse equation is already correct
  return (pow((touchX - centerX), 2) / pow(a, 2) + pow((touchY - centerY), 2) / pow(b, 2) <= 1);
}

void Ellipse::printDetails(bool shouldPrint) {
  if (!shouldPrint) return;
  Shape::printDetails(shouldPrint);
  writeLog("Ellipse: CenterX=%d, CenterY=%d, a=%d, b=%d", centerX, centerY, a, b);
}

// Circle implementation
Circle::Circle(String _name, int _centerX, int _centerY, int _radius)
  : Ellipse(_name, _centerX, _centerY, _radius, _radius), radius(_radius) {}

bool Circle::isTouched(int16_t touchX, int16_t touchY) {
  // Circle distance calculation is already correct
  float distance = sqrt(pow(touchX - centerX, 2) + pow(touchY - centerY, 2));
  return (distance <= radius);
}

void Circle::printDetails(bool shouldPrint) {
  if (!shouldPrint) return;
  Shape::printDetails(shouldPrint);
  writeLog("Circle: CenterX=%d, CenterY=%d, Radius=%d", centerX, centerY, radius);
}
