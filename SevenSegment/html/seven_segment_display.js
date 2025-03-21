
class SevenSegmentDisplay
{
	constructor(SVGID)
	{
        this._ColorSchemes =
        {
            LCD: 1,
            Green: 2,
            Orange: 3,
            Blue: 4,
            Sky: 5,
            Red: 6
        };

        this._DecimalPointTypes =
        {
            Floating: 1,
            Fixed: 2
        };

        this._NumberOfDigits = 8;
        this._Value = 0;
        this._BackgroundColor = "#E0E0E0";
        this._LitSegmentColor = "#202020";
        this._UnlitSegmentColor = "#D0D0D0";
        this._ColorScheme = this._ColorSchemes.LCD;
        this._SVGID = SVGID;
        this._DecimalPointType = this._DecimalPointTypes.Floating;
        this._NumberOfDecimalPlaces = 2;
        this._ValueDisplayString = "0";
        this._PreviousValueDisplayString = "";
        this._DecimalPointPosition = -1;
        this._Paths;
        this._Background;

        this._Segments = [];

        // Set segment on/off flags for digits 0 to 9
        this._Segments[0] = [1, 1, 1, 0, 1, 1, 1];
        this._Segments[1] = [0, 0, 1, 0, 0, 1, 0];
        this._Segments[2] = [1, 0, 1, 1, 1, 0, 1];
        this._Segments[3] = [1, 0, 1, 1, 0, 1, 1];
        this._Segments[4] = [0, 1, 1, 1, 0, 1, 0];
        this._Segments[5] = [1, 1, 0, 1, 0, 1, 1];
        this._Segments[6] = [1, 1, 0, 1, 1, 1, 1];
        this._Segments[7] = [1, 0, 1, 0, 0, 1, 0];
        this._Segments[8] = [1, 1, 1, 1, 1, 1, 1];
        this._Segments[9] = [1, 1, 1, 1, 0, 1, 0];
        // blank
        this._Segments[10] = [0, 0, 0, 0, 0, 0, 0];
        // Minus sign
        this._Segments[11] = [0, 0, 0, 1, 0, 0, 0];

        for (let i = 1; i <this._NumberOfDigits; i++)
        {
            this._ValueDisplayString = " " + this._ValueDisplayString;
        }

        this._SetHeight();

        this._CreateSegments();

        this._SetSegmentColours(false);
	}

    //---------------------------------------------------------------
    // PROPERTIES
    //---------------------------------------------------------------

	get ColorSchemes() { return this._ColorSchemes; }

	get DecimalPointTypes() { return this._DecimalPointTypes; }

	get BackgroundColor() { return this._BackgroundColor; }
	set BackgroundColor(BackgroundColor)
    {
        this._BackgroundColor = BackgroundColor;
        this._Background.setAttributeNS(null, 'style', "fill:" + this._BackgroundColor + ";");
    }

	get LitSegmentColor() { return this._LitSegmentColor; }
	set LitSegmentColor(LitSegmentColor)
    {
        this._LitSegmentColor = LitSegmentColor;
        this._SetSegmentColours(true);
    }

	get UnlitSegmentColor() { return this._UnlitSegmentColor; }
	set UnlitSegmentColor(UnlitSegmentColor)
    {
        this._UnlitSegmentColor = UnlitSegmentColor;
        this._SetSegmentColours(true);
    }

	get ColorScheme() { return this._ColorScheme; }
	set ColorScheme(ColorScheme)
    {
        this._ColorScheme = ColorScheme;

        switch (this._ColorScheme)
        {
            // LCD
            case 1:
                this._BackgroundColor = "#E0E0E0";
                this._LitSegmentColor = "#202020";
                this._UnlitSegmentColor = "#D0D0D0";
                break;

            // Green
            case 2:
                this._BackgroundColor = "#002000";
                this._LitSegmentColor = "#00FF00";
                this._UnlitSegmentColor = "#003000";
                break;

            // Orange
            case 3:
                this._BackgroundColor = "#000000";
                this._LitSegmentColor = "#FF8000";
                this._UnlitSegmentColor = "#201000";
                break;

            // Blue
            case 4:
                this._BackgroundColor = "#000020";
                this._LitSegmentColor = "#0000FF";
                this._UnlitSegmentColor = "#000030";
                break;

            // Sky
            case 5:
                this._BackgroundColor = "#00FFFF";
                this._LitSegmentColor = "#0000FF";
                this._UnlitSegmentColor = "#00DFDF";
                break;

            // Red
            case 6:
                this._BackgroundColor = "#200000";
                this._LitSegmentColor = "#FF0000";
                this._UnlitSegmentColor = "#300000";
                break;

            default:
                break;
        }

        this._Background.setAttributeNS(null, 'style', "fill:" + this._BackgroundColor + ";");
        this._SetSegmentColours(true);
    }

	get NumberOfDigits() { return this._NumberOfDigits; }
	set NumberOfDigits(NumberOfDigits)
    {
				console.log("set NumberOfDigits NumberOfDigits=", NumberOfDigits);
        let ivalue = parseInt(NumberOfDigits);

        if (isNaN(ivalue) || ivalue == null || ivalue < 1 || ivalue > 12)
        {
            throw new Error("NumberOfDigits must be an integer between 1 and 12");
        }
        else
        {
            this._NumberOfDigits = NumberOfDigits;
            this._SetHeight();
            this._CalculateValueDisplayString();
            this._CreateSegments();
            this._SetSegmentColours(true);
        }
    }

	get NumberOfDecimalPlaces() { return this._NumberOfDecimalPlaces; }
	set NumberOfDecimalPlaces(NumberOfDecimalPlaces)
    {
        let ivalue = parseInt(NumberOfDecimalPlaces);

        if (isNaN(ivalue) || ivalue == null || ivalue < 0 || ivalue > 10)
        {
            throw new Error("NumberOfDecimalPlaces must be an integer between 0 and 10");
        }
        else
        {
            this._NumberOfDecimalPlaces = NumberOfDecimalPlaces;
            this._CalculateValueDisplayString();
            this._SetSegmentColours(true);
        }
    }

	get DecimalPointType() { return this._DecimalPointType; }
	set DecimalPointType(DecimalPointType)
    {
        this._DecimalPointType = DecimalPointType;
        this._CalculateValueDisplayString();
        this._SetSegmentColours(true);
    }

	get Value() { return this._Value; }
	set Value(Value)
    {
				//console.log("set Value");
        let fvalue = parseFloat(Value);
        let sivalue = parseInt(Value).toString();

        if (isNaN(fvalue))
        {
            throw new Error("Value must be a real number");
        }
        else if (sivalue.length > this._NumberOfDigits)
        {
            throw new Error("Length of integer portion of Value is longer than NumberOfDigits");
        }
        else
        {
            this._Value = Value;
            this._CalculateValueDisplayString();
            this._SetSegmentColours(false);
        }
    }

    //---------------------------------------------------------------
    // METHODS
    //---------------------------------------------------------------

    _SetHeight()
    {
        document.getElementById(this._SVGID).setAttribute("height", (document.getElementById(this._SVGID).getClientRects()[0].width) / (this._NumberOfDigits * 0.6));
    }

    _CalculateValueDisplayString()
    {
        if (this._DecimalPointType == 2)
        {
            this._ValueDisplayString = this._Value.toFixed(this._NumberOfDecimalPlaces).toString();
        }
        else
        {
            this._ValueDisplayString = this._Value.toString();
        }

        this._DecimalPointPosition = this._ValueDisplayString.indexOf(".");

        this._ValueDisplayString = this._ValueDisplayString.replace(".", "");

        // pad left
        for (let i = this._ValueDisplayString.length + 1; i <= this._NumberOfDigits; i++)
        {
            this._ValueDisplayString = " " + this._ValueDisplayString;

            if (this._DecimalPointPosition > -1)
            {
                this._DecimalPointPosition++;
            }
        }
    }

    _CreateSegments()
    {
        document.getElementById(this._SVGID).innerHTML = "";

        // Create Background
        let rect = document.createElementNS("http://www.w3.org/2000/svg", 'rect');

        rect.setAttributeNS(null, 'x', 0);
        rect.setAttributeNS(null, 'y', 0);
        rect.setAttributeNS(null, 'height', document.getElementById(this._SVGID).getClientRects()[0].height);
        rect.setAttributeNS(null, 'width', document.getElementById(this._SVGID).getClientRects()[0].width);
        rect.setAttributeNS(null, 'style', "fill:" + this._BackgroundColor + ";");

        document.getElementById(this._SVGID).appendChild(rect);

        this._Background = rect;

        // Create paths
        this._Paths = [];

        for (let i = 0; i < this._NumberOfDigits; i++)
        {
            let SVGWidth = document.getElementById(this._SVGID).getClientRects()[0].width;
            let SVGHeight = document.getElementById(this._SVGID).getClientRects()[0].height;
            let DigitWidth = SVGWidth / this._NumberOfDigits;
            let Unit = SVGHeight / 40.0;
            let x = DigitWidth * i;

            let d = "";
            let path;

            this._Paths[i] = [];

            // TOP
            d += "M" + (x + (Unit * 6)) + "," + (Unit * 3) + " ";
            d += "L" + (x + (Unit * 21)) + "," + (Unit * 3) + " ";
            d += "L" + (x + (Unit * 18)) + "," + (Unit * 6) + " ";
            d += "L" + (x + (Unit * 9)) + "," + (Unit * 6) + " ";
            d += "Z";
            this._Paths[i][0] = document.createElementNS("http://www.w3.org/2000/svg", "path");
            this._Paths[i][0].setAttributeNS(null, "d", d);
            this._Paths[i][0].setAttributeNS(null, "style", "fill:" + this._UnlitSegmentColor + ";");
            document.getElementById(this._SVGID).appendChild(this._Paths[i][0]);
            d = "";

            // TOP LEFT
            d += "M" + (x + (Unit * 5)) + "," + (Unit * 4) + " ";
            d += "L" + (x + (Unit * 8)) + "," + (Unit * 7) + " ";
            d += "L" + (x + (Unit * 8)) + "," + (Unit * 16) + " ";
            d += "L" + (x + (Unit * 5)) + "," + (Unit * 19) + " ";
            d += "Z";
            this._Paths[i][1] = document.createElementNS("http://www.w3.org/2000/svg", "path");
            this._Paths[i][1].setAttributeNS(null, "d", d);
            this._Paths[i][1].setAttributeNS(null, "style", "fill:" + this._UnlitSegmentColor + ";");
            document.getElementById(this._SVGID).appendChild(this._Paths[i][1]);
            d = "";

            // TOP RIGHT
            d += "M" + (x + (Unit * 22)) + "," + (Unit * 4) + " ";
            d += "L" + (x + (Unit * 22)) + "," + (Unit * 19) + " ";
            d += "L" + (x + (Unit * 19)) + "," + (Unit * 16) + " ";
            d += "L" + (x + (Unit * 19)) + "," + (Unit * 7) + " ";
            d += "Z";
            this._Paths[i][2] = document.createElementNS("http://www.w3.org/2000/svg", "path");
            this._Paths[i][2].setAttributeNS(null, "d", d);
            this._Paths[i][2].setAttributeNS(null, "style", "fill:" + this._UnlitSegmentColor + ";");
            document.getElementById(this._SVGID).appendChild(this._Paths[i][2]);
            d = "";

            // MIDDLE
            d += "M" + (x + (Unit * 6)) + "," + (Unit * 20) + " ";
            d += "L" + (x + (Unit * 8)) + "," + (Unit * 18) + " ";
            d += "L" + (x + (Unit * 19)) + "," + (Unit * 18) + " ";
            d += "L" + (x + (Unit * 21)) + "," + (Unit * 20) + " ";
            d += "L" + (x + (Unit * 19)) + "," + (Unit * 22) + " ";
            d += "L" + (x + (Unit * 8)) + "," + (Unit * 22) + " ";
            d += "Z";
            this._Paths[i][3] = document.createElementNS("http://www.w3.org/2000/svg", "path");
            this._Paths[i][3].setAttributeNS(null, "d", d);
            this._Paths[i][3].setAttributeNS(null, "style", "fill:" + this._UnlitSegmentColor + ";");
            document.getElementById(this._SVGID).appendChild(this._Paths[i][3]);
            d = "";

            // BOTTOM LEFT
            d += "M" + (x + (Unit * 5)) + "," + (Unit * 21) + " ";
            d += "L" + (x + (Unit * 8)) + "," + (Unit * 24) + " ";
            d += "L" + (x + (Unit * 8)) + "," + (Unit * 33) + " ";
            d += "L" + (x + (Unit * 5)) + "," + (Unit * 36) + " ";
            d += "Z";
            this._Paths[i][4] = document.createElementNS("http://www.w3.org/2000/svg", "path");
            this._Paths[i][4].setAttributeNS(null, "d", d);
            this._Paths[i][4].setAttributeNS(null, "style", "fill:" + this._UnlitSegmentColor + ";");
            document.getElementById(this._SVGID).appendChild(this._Paths[i][4]);
            d = "";

            // BOTTOM RIGHT
            d += "M" + (x + (Unit * 22)) + "," + (Unit * 21) + " ";
            d += "L" + (x + (Unit * 22)) + "," + (Unit * 36) + " ";
            d += "L" + (x + (Unit * 19)) + "," + (Unit * 33) + " ";
            d += "L" + (x + (Unit * 19)) + "," + (Unit * 24) + " ";
            d += "Z";
            this._Paths[i][5] = document.createElementNS("http://www.w3.org/2000/svg", "path");
            this._Paths[i][5].setAttributeNS(null, "d", d);
            this._Paths[i][5].setAttributeNS(null, "style", "fill:" + this._UnlitSegmentColor + ";");
            document.getElementById(this._SVGID).appendChild(this._Paths[i][5]);
            d = "";

            // BOTTOM
            d += "M" + (x + (Unit * 6)) + "," + (Unit * 37) + " ";
            d += "L" + (x + (Unit * 9)) + "," + (Unit * 34) + " ";
            d += "L" + (x + (Unit * 18)) + "," + (Unit * 34) + " ";
            d += "L" + (x + (Unit * 21)) + "," + (Unit * 37) + " ";
            d += "Z";
            this._Paths[i][6] = document.createElementNS("http://www.w3.org/2000/svg", "path");
            this._Paths[i][6].setAttributeNS(null, "d", d);
            this._Paths[i][6].setAttributeNS(null, "style", "fill:" + this._UnlitSegmentColor + ";");
            document.getElementById(this._SVGID).appendChild(this._Paths[i][6]);
            d = "";

            // DECIMAL POINT
            d += "M" + x + "," + (Unit * 34) + " ";
            d += "L" + (x + (Unit * 3)) + "," + (Unit * 34) + " ";
            d += "L" + (x + (Unit * 3)) + "," + (Unit * 37) + " ";
            d += "L" + x + "," + (Unit * 37) + " ";
            d += "Z";
            this._Paths[i][7] = document.createElementNS("http://www.w3.org/2000/svg", "path");
            this._Paths[i][7].setAttributeNS(null, "d", d);
            this._Paths[i][7].setAttributeNS(null, "style", "fill:" + this._UnlitSegmentColor + ";");
            document.getElementById(this._SVGID).appendChild(this._Paths[i][7]);
        }
    }

    _SetSegmentColours(RedrawAll)
    {
        if (RedrawAll == true)
            this._PreviousValueDisplayString = "";

        let s;
        let Digit;

        for (let i = 0; i < this._NumberOfDigits; i++)
        {
            if (this._ValueDisplayString[i] != this._PreviousValueDisplayString[i])
            {
                Digit = parseInt(this._ValueDisplayString[i]);

                if (isNaN(Digit))
                {
                    if (this._ValueDisplayString[i] == "-")
                        Digit = 11;
                    else
                        Digit = 10;
                }

                for (s = 0; s <= 6; s++)
                {
                    if (this._Segments[Digit][s] == 1)
                        this._Paths[i][s].setAttributeNS(null, 'style', "fill:" + this._LitSegmentColor + ";");
                    else
                        this._Paths[i][s].setAttributeNS(null, 'style', "fill:" + this._UnlitSegmentColor + ";");
                }
            }

            if (i == this._DecimalPointPosition)
                this._Paths[i][7].setAttributeNS(null, 'style', "fill:" + this._LitSegmentColor + ";");
            else
                this._Paths[i][7].setAttributeNS(null, 'style', "fill:" + this._UnlitSegmentColor + ";");
        }

        this._PreviousValueDisplayString = this._ValueDisplayString;
    }
}
