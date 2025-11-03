#ifndef SHAPES_H
#define SHAPES_H

#include <Arduino.h>

// Base Shape class
class Shape {
public:
  String name;
  Shape(String _name);
  virtual bool isTouched(int16_t touchX, int16_t touchY) = 0;  // Pure virtual function
  virtual void printDetails(bool shouldPrint = false);
  virtual int getX() = 0;  // Pure virtual function for X coordinate
  virtual int getY() = 0;  // Pure virtual function for Y coordinate
  virtual void setX(int x) = 0;  // Pure virtual function to set X
  virtual void setY(int y) = 0;  // Pure virtual function to set Y
};

// Rectangle class
class Rectangle : public Shape {
private:
  int x, y;
public:
  int width, height;
  Rectangle(String _name, int _x, int _y, int _width, int _height);
  bool isTouched(int16_t touchX, int16_t touchY) override;
  void printDetails(bool shouldPrint = false) override;
  int getX() override { return x; }
  int getY() override { return y; }
  void setX(int _x) override { x = _x; }
  void setY(int _y) override { y = _y; }
};

// Ellipse class
class Ellipse : public Shape {
protected:
  int centerX, centerY;
public:
  int a, b;  // semi-major axis, semi-minor axis
  Ellipse(String _name, int _centerX, int _centerY, int _a, int _b);
  bool isTouched(int16_t touchX, int16_t touchY) override;
  void printDetails(bool shouldPrint = false) override;
  int getX() override { return centerX; }
  int getY() override { return centerY; }
  void setX(int x) override { centerX = x; }
  void setY(int y) override { centerY = y; }
};

// Circle class
class Circle : public Ellipse {
public:
  int radius;
  Circle(String _name, int _centerX, int _centerY, int _radius);
  bool isTouched(int16_t touchX, int16_t touchY) override;
  void printDetails(bool shouldPrint = false) override;
};

#endif // SHAPES_H