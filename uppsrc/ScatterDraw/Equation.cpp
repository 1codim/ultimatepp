#include "ScatterDraw.h"

#include <plugin/Eigen/Eigen.h>

NAMESPACE_UPP

using namespace Eigen;

struct Equation_functor : NonLinearOptimizationFunctor<double> {
	DataSource *series;
	ExplicitEquation *fSource;
	Equation_functor() {}
	
	int operator()(const VectorXd &b, VectorXd &fvec) const {
		ASSERT(b.size() == unknowns);
		ASSERT(fvec.size() == datasetLen);
		for (int i = 0; i < unknowns; ++i)
			(*fSource).SetCoeffVal(i, b(i));
		for(int64 i = 0; i < datasetLen; i++) 
			fvec(ptrdiff_t(i)) = (*fSource).f((*series).x(i)) - (*series).y(i);
		return 0;
	}
};

void ExplicitEquation::SetNumCoeff(int num) {
	coeff.SetCount(num); 
	for (int i = 0; i < num; ++i)
		coeff[i] = 0;
}

ExplicitEquation::FitError ExplicitEquation::Fit(DataSource &series, double &r2) {
	r2 = 0;
	if (series.IsExplicit() || series.IsParam())
		return InadequateDataSource;
	
	if (series.GetCount() < coeff.GetCount())
		return SmallDataSource;
	
	ptrdiff_t numUnknowns = coeff.GetCount();
	
	VectorXd x(numUnknowns);
	for (int i = 0; i < numUnknowns; ++i)
		x(i) = coeff[i];
	
	Equation_functor functor;	
	functor.series = &series;
	functor.fSource = this;
	functor.unknowns = numUnknowns;
	functor.datasetLen = series.GetCount();
	
	NumericalDiff<Equation_functor> numDiff(functor);
	LevenbergMarquardt<NumericalDiff<Equation_functor> > lm(numDiff);
// 	ftol is a nonnegative input variable that measures the relative error desired in the sum of squares 
	lm.parameters.ftol = 1.E4*NumTraits<double>::epsilon();
//  xtol is a nonnegative input variable that measures the relative error desired in the approximate solution
	lm.parameters.xtol = 1.E4*NumTraits<double>::epsilon();
	lm.parameters.maxfev = maxFitFunctionEvaluations;
	int ret = lm.minimize(x);
	if (ret == LevenbergMarquardtSpace::ImproperInputParameters)
		return ExplicitEquation::ImproperInputParameters;
	if (ret == LevenbergMarquardtSpace::TooManyFunctionEvaluation)
		return TooManyFunctionEvaluation;

	double mean = series.AvgY();
	double sse = 0, sst = 0;
	for (int64 i = 0; i < series.GetCount(); ++i) {
		double y = series.y(i);
		if (!IsNull(y)) {
			double res = y - f(series.x(i));
			sse += res*res;
			double d = y - mean;
			sst += d*d;
		}
	}
	r2 = 1 - sse/sst;

	return NoError;
}

int ExplicitEquation::maxFitFunctionEvaluations = 1000;


double PolynomialEquation::f(double x) {
	double y = 0;
	for (int i = 0; i < coeff.GetCount(); ++i) 
		y += coeff[i]*pow(x, i);
	return y;
}

String PolynomialEquation::GetEquation(int numDigits) {
	if (coeff.IsEmpty())
		return String();
	String ret = FormatCoeff(0, numDigits);
	if (coeff.GetCount() == 1)
		return ret;
	ret += Format(" + %s*x", FormatCoeff(1, numDigits));
	for (int i = 2; i < coeff.GetCount(); ++i) 
		ret += Format(" + %s*x^%s", FormatCoeff(i, numDigits), FormatInt(i));
	ret.Replace("+ -", "- ");
	return ret;
}
	
double FourierEquation::f(double x) {
	double y = coeff[0];
	double w = coeff[1];
	for (int i = 2; i < coeff.GetCount(); i += 2) {
		int n = 1 + (i - 2)/2;
		y += coeff[i]*cos(n*w*x) + coeff[i+1]*sin(n*w*x);
	}
	return y;
}

String FourierEquation::GetEquation(int numDigits) {
	if (coeff.GetCount() < 4)
		return String();
	String ret = FormatCoeff(0, numDigits);
	
	for (int i = 2; i < coeff.GetCount(); i += 2) {
		int n = 1 + (i - 2)/2;
		String nwx = Format("%d*%s*x", n, FormatCoeff(1, numDigits));
		ret += Format(" + %s*cos(%s)", FormatCoeff(i, numDigits), nwx);
		ret += Format(" + %s*sin(%s)", FormatCoeff(i + 1, numDigits), nwx);
	}
	ret.Replace("+ -", "- ");
	return ret;
}

static double degToRad(double deg) {return M_PI/180.;}

EvalExpr::EvalExpr() {
	noCase = false;
	
	constants.Add("PI", M_PI);
	constants.Add("M_PI", M_PI);
	constants.Add("e", M_E);
	
	functions.Add("abs", fabs);
	functions.Add("ceil", ceil);
	functions.Add("floor", floor);
	functions.Add("sqrt", sqrt);
	functions.Add("sin", sin);
	functions.Add("cos", cos);
	functions.Add("tan", tan);
	functions.Add("asin", asin);
	functions.Add("acos", acos);
	functions.Add("atan", atan);
	functions.Add("sinh", sinh);
	functions.Add("cosh", cosh);
	functions.Add("tanh", tanh);
	functions.Add("log", log);
	functions.Add("log10", log10);
	functions.Add("exp", exp);
	functions.Add("degToRad", degToRad);
}

double EvalExpr::Term(CParser& p) {
	if(p.IsId()) {
		String strId = p.ReadId();
		if(double (*function)(double) = functions.Get(strId, 0)) {
			p.PassChar('(');
			double x = Exp(p);
			p.PassChar(')');
			return function(x);
		}	
		if (noCase)
			strId = ToUpper(strId);
		double ret = constants.Get(strId, Null);
		if (IsNull(ret))
			ret = variables.GetAdd(strId, 0);
		return ret;
	} else if(p.Char('(')) {
		double x = Exp(p);
		p.PassChar(')');
		return x;
	} else
		return p.ReadDouble();
}

double EvalExpr::Pow(CParser& p) {
	double x = Term(p);
	for(;;) {
		if(p.Char('^'))
			x = pow(x, Term(p));
		else
			return x;
	}
}

double EvalExpr::Mul(CParser& p) {
	double x = Pow(p);
	for(;;) {
		if(p.Char('*'))
			x = x * Mul(p);
		else if(p.Char('/')) {
			double y = Pow(p);
			if(y == 0)
				p.ThrowError(t_("Divide by zero"));
			x = x / y;
		} else
			return x;
	}
}

double EvalExpr::Exp(CParser& p) {
	double x = Mul(p);
	for(;;) {
		if(p.Char('+'))
			x = x + Mul(p);
		else if(p.Char('-'))
			x = x - Mul(p);
		else if(p.Char(':'))
			x = x*60 + Mul(p);
		else
			return x;
	}
}

double EvalExpr::Eval(String line) {
	line = TrimBoth(line);
	if (line.IsEmpty())
		return Null;
	
	CParser p(line);
	try {
		if(p.IsId()) {
			CParser::Pos pos = p.GetPos();
			String var = p.ReadId();
			if(p.Char('=')) {
				double ret = Exp(p);
				variables.GetAdd(var) = ret;
				return ret;
			} else {
				p.SetPos(pos);
				return Exp(p);
			}
		} else
			return Exp(p);
	}
	catch(CParser::Error e) {
		LOG(Format(t_("Error evaluating '%s': %s"), line, e));
		return Null;
	}
}

String EvalExpr::TermStr(CParser& p, int numDigits) {
	if(p.IsId()) {
		String strId = p.ReadId();
		if(functions.Find(strId) >= 0) {
			p.PassChar('(');
			String x = ExpStr(p, numDigits);
			p.PassChar(')');
			return strId + "(" + x + ")";
		}
		if (noCase)
			strId = ToUpper(strId);
		if (IsNull(numDigits)) {
			if (constants.Find(strId) < 0)
				variables.GetAdd(strId, 0);
			return strId;
		} else {
			if (constants.Find(strId) >= 0)
				return strId;
			else {
				double dat = variables.GetAdd(strId, 0);
				return FormatDoubleFix(dat, numDigits);
			}
		}
	}
	if(p.Char('(')) {
		String x = ExpStr(p, numDigits);
		p.PassChar(')');
		return "(" + x + ")";
	}
	return FormatDoubleFix(p.ReadDouble(), IsNull(numDigits) ? 3 : numDigits);
}

String EvalExpr::PowStr(CParser& p, int numDigits) {
	String x = TermStr(p, numDigits);
	for(;;)
		if(p.Char('^'))
			x = x + "^" + TermStr(p, numDigits);
		else
			return x;
}

String EvalExpr::MulStr(CParser& p, int numDigits) {
	String x = PowStr(p, numDigits);
	for(;;)
		if(p.Char('*'))
			x = x + "*" + MulStr(p, numDigits);
		else if(p.Char('/')) 
			x = x + "/" + PowStr(p, numDigits);
		else
			return x;
}

String EvalExpr::ExpStr(CParser& p, int numDigits) {
	String x = MulStr(p, numDigits);
	for(;;) {
		if(p.Char('+'))
			x = x + " + " + MulStr(p, numDigits);
		else if(p.Char('-'))
			x = x + " - " + MulStr(p, numDigits);
		else if(p.Char(':'))
			x = x + ":" + MulStr(p, numDigits);
		else {
			x.Replace("+ -", "- ");
			return x;
		}
	}
}

String EvalExpr::EvalStr(String line, int numDigits) {
	line = TrimBoth(line);
	if (line.IsEmpty())
		return Null;
	
	CParser p(line);
	try {
		if(p.IsId()) {
			CParser::Pos pos = p.GetPos();
			String var = p.ReadId();
			if(p.Char('=')) {
				String ret = ExpStr(p, numDigits);
				variables.GetAdd(var, 0);
				return var + " = " + ret;
			} else {
				p.SetPos(pos);
				return ExpStr(p, numDigits);
			}
		} else
			return ExpStr(p, numDigits);
	}
	catch(CParser::Error e) {
		LOG(Format(t_("Error evaluating '%s': %s"), line, e));
		return Null;
	}
}


ExplicitEquation::FitError SplineEquation::Fit(DataSource &data, double &r2)
{	
	Vector<Pointf> seriesRaw;
	for (int64 i = 0; i < data.GetCount(); ++i) {		// Remove Nulls	
		if (!IsNull(data.x(i)) && !IsNull(data.y(i)))
			seriesRaw << Pointf(data.x(i), data.y(i));
	}

	if(seriesRaw.IsEmpty())
        return InadequateDataSource;
        	
	PointfLess less;
	Sort(seriesRaw, less);								// Sort
	
	Vector<Pointf> series;
	for (int i = 1; i < seriesRaw.GetCount(); ++i) {	// Remove points with duplicate x
		if (seriesRaw[i].x != seriesRaw[i - 1].x)
			series << seriesRaw[i];	
	}

	r2 = 0;
	
    int n = int(series.GetCount()) - 1;
    
    Buffer<double> h(n);
    for(int i = 0; i < n; ++i)
        h[i] = (series[i+1].x - series[i].x);

    Buffer<double> alpha(n);
    for(int i = 1; i < n; ++i)
        alpha[i] = (3.*(series[i+1].y - series[i].y)/h[i] - 3*(series[i].y - series[i-1].y)/h[i-1]);

    Buffer<double> c(n+1), l(n+1), mu(n+1), z(n+1);
    l[0] = 1;
    mu[0] = 0;
    z[0] = 0;

    for(int i = 1; i < n; ++i) {
        l[i] = 2.*(series[i+1].x - series[i-1].x) - h[i-1]*mu[i-1];
        mu[i] = h[i]/l[i];
        z[i] = (alpha[i] - h[i-1]*z[i-1])/l[i];
    }

    l[n] = 1.;
    z[n] = 0;
    c[n] = 0;

	Buffer<double> b(n), d(n);
    for(int i = n-1; i >= 0; --i) {
        c[i] = z[i] - mu[i] * c[i+1];
        b[i] = (series[i+1].y - series[i].y)/h[i] - h[i]*(c[i+1] + 2*c[i])/3.;
        d[i] = (c[i+1] - c[i])/3./h[i];
    }

	ncoeff = n;
    coeff.Alloc(ncoeff);
    for(int i = 0; i < n; ++i) {
        coeff[i].x = series[i].x;
        coeff[i].a = series[i].y;
        coeff[i].b = b[i];
        coeff[i].c = c[i];
        coeff[i].d = d[i];
    }
    return NoError;
}

double SplineEquation::f(double x)
{
    int j;
    for (j = 0; j < ncoeff; j++) {
        if(coeff[j].x > x) {
            if(j == 0)
                j++;
            break;
        }
    }
    j--;

    double dx = x - coeff[j].x;
    return coeff[j].a + coeff[j].b*dx + coeff[j].c*dx*dx + coeff[j].d*dx*dx*dx;
}

INITBLOCK {
	ExplicitEquation::Register<LinearEquation>("LinearEquation");
	ExplicitEquation::Register<PolynomialEquation2>("PolynomialEquation2");
	ExplicitEquation::Register<PolynomialEquation3>("PolynomialEquation3");
	ExplicitEquation::Register<PolynomialEquation4>("PolynomialEquation4");
	ExplicitEquation::Register<PolynomialEquation5>("PolynomialEquation5");
	ExplicitEquation::Register<SinEquation>("SinEquation");
	ExplicitEquation::Register<ExponentialEquation>("ExponentialEquation");
	ExplicitEquation::Register<Rational1Equation>("Rational1Equation");
	ExplicitEquation::Register<FourierEquation1>("FourierEquation1");
	ExplicitEquation::Register<FourierEquation2>("FourierEquation2");
	ExplicitEquation::Register<FourierEquation3>("FourierEquation3");
	ExplicitEquation::Register<FourierEquation4>("FourierEquation4");
}

END_UPP_NAMESPACE