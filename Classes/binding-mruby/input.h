#ifndef INPUT_H
#define INPUT_H

struct InputPrivate;

class Input
{
public:
	enum ButtonCode
	{
		None = 0,

		Down = 2, Left = 4, Right = 6, Up = 8,

		A = 11, B = 12, C = 13,
		X = 14, Y = 15, Z = 16,
		L = 17, R = 18,

		Shift = 21, Ctrl = 22, Alt = 23,

		F5 = 25, F6 = 26, F7 = 27, F8 = 28, F9 = 29,

		/* Non-standard extensions */
		MouseLeft = 38, MouseMiddle = 39, MouseRight = 40
	};

	static const int buttonCodeSize;

	void update();

	bool isPressed(int button);
	bool isTriggered(int button);
	bool isRepeated(int button);

	int dir4Value();
	int dir8Value();

	/* Non-standard extensions */
	int mouseX();
	int mouseY();

private:
	Input();
	~Input();

	friend struct SharedStatePrivate;

	InputPrivate *p;
};

#endif // INPUT_H
