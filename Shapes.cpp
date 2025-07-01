#include "Shapes.h"

// Shape implementation
Shape::Shape(String _name) : name(_name) {}

void Shape::printDetails() {
  Serial.print("Shape: Name = ");
  Serial.println(name);
}

// Rectangle implementation
Rectangle::Rectangle(String _name, int _x, int _y, int _width, int _height)
  : Shape(_name), x(_x), y(_y), width(_width), height(_height) {}

bool Rectangle::isTouched(int16_t touchX, int16_t touchY) {
  return (touchX >= x && touchX <= (x + width) && touchY >= y && touchY <= (y + height));
}

void Rectangle::printDetails() {
  Shape::printDetails();
  Serial.print("Rectangle: X = ");
  Serial.print(x);
  Serial.print(", Y = ");
  Serial.print(y);
  Serial.print(", Width = ");
  Serial.print(width);
  Serial.print(", Height = ");
  Serial.println(height);
}

// Ellipse implementation
Ellipse::Ellipse(String _name, int _centerX, int _centerY, int _a, int _b)
  : Shape(_name), centerX(_centerX), centerY(_centerY), a(_a), b(_b) {}

bool Ellipse::isTouched(int16_t touchX, int16_t touchY) {
  // Ellipse equation is already correct
  return (pow((touchX - centerX), 2) / pow(a, 2) + pow((touchY - centerY), 2) / pow(b, 2) <= 1);
}

void Ellipse::printDetails() {
  Shape::printDetails();
  Serial.print("Ellipse: CenterX = ");
  Serial.print(centerX);
  Serial.print(", CenterY = ");
  Serial.print(centerY);
  Serial.print(", a = ");
  Serial.print(a);
  Serial.print(", b = ");
  Serial.println(b);
}

// Circle implementation
Circle::Circle(String _name, int _centerX, int _centerY, int _radius)
  : Ellipse(_name, _centerX, _centerY, _radius, _radius), radius(_radius) {}

bool Circle::isTouched(int16_t touchX, int16_t touchY) {
  // Circle distance calculation is already correct
  float distance = sqrt(pow(touchX - centerX, 2) + pow(touchY - centerY, 2));
  return (distance <= radius);
}

void Circle::printDetails() {
  Shape::printDetails();
  Serial.print("Circle: CenterX = ");
  Serial.print(centerX);
  Serial.print(", CenterY = ");
  Serial.print(centerY);
  Serial.print(", Radius = ");
  Serial.println(radius);
}
