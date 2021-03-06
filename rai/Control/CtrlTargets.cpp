/*  ------------------------------------------------------------------
    Copyright (c) 2011-2020 Marc Toussaint
    email: toussaint@tu-berlin.de

    This code is distributed under the MIT License.
    Please see <root-path>/LICENSE for details.
    --------------------------------------------------------------  */

#include "CtrlTargets.h"

#include "control.h"

//===========================================================================

ActStatus CtrlTarget_Const::step(double tau, CtrlObjective* o, const ConfigurationL& Ctuple) {
  o->feat->target.clear();
  return AS_running;
}

//===========================================================================

ActStatus CtrlTarget_ConstVel::step(double tau, CtrlObjective* o, const ConfigurationL& Ctuple) {
  o->feat->target.clear();
  return AS_running;
}

//===========================================================================

CtrlTarget_MaxCarrot::CtrlTarget_MaxCarrot(CtrlObjective& co, double maxDistance, const arr& _goal) : maxDistance(maxDistance) {
  if(!!_goal) goal=_goal;
//  isTransient = true;
}

uint getRealValue(CtrlObjective* o, arr& y_raw, arr& y_real, const ConfigurationL& Ctuple) {
  //get the current (scaled) feature value
  y_real =  o->feat->phi(Ctuple);
  if(o->type==OT_ineq) for(double& d:y_real) if(d<0.) d=0.;

  //what's the raw (unscaled) y-dimension?
  uint d_raw = y_real.N;
  if(o->feat->scale.nd==2) d_raw = o->feat->scale.d1;

  //get the raw y-value (before scaling; also projected in scale's input space)
  y_raw = y_real;
  if(o->feat->scale.N==1) y_raw /= o->feat->scale.scalar();
  else if(o->feat->scale.nd==1) y_raw /= o->feat->scale;
  else if(o->feat->scale.nd==2) y_raw = pseudoInverse(o->feat->scale)*y_real;
  if(o->feat->target.N) y_raw += o->feat->target;

  CHECK_EQ(d_raw, y_raw.N, "");

  return d_raw;
}

ActStatus CtrlTarget_MaxCarrot::step(double tau, CtrlObjective* o, const ConfigurationL& Ctuple) {
  arr y_real, y_raw;

  uint d_raw = getRealValue(o, y_raw, y_real, Ctuple);

  //initialize goal
  if(goal.N!=d_raw) {
    if(o->feat->target.N==d_raw) goal = o->feat->target;
    else goal = zeros(d_raw);
  }

  double d = length(y_raw-goal);
  if(d > maxDistance) {
    o->feat->target = y_raw - (maxDistance/d)*(y_raw-goal);
    //cout << "maxD" << endl;
    isTransient=true;
  } else {
    o->feat->target = goal;
    isTransient=false;
  }
  if(d<maxDistance) countInRange++; else countInRange=0;
  if(countInRange>10) return AS_converged;
  return AS_running;
}

//===========================================================================

CtrlTarget_PathCarrot::CtrlTarget_PathCarrot(const arr& path, double maxDistance, double _endTime)
  : maxStep(maxDistance), endTime(_endTime), time(0.) {
  CHECK_EQ(path.nd, 2, "need a properly shaped path!");
  arr times(path.d0);
  for(uint i=0; i<path.d0; i++) times.elem(i) = endTime*double(i)/double(times.N-1);
  spline.set(2, path, times);
}

CtrlTarget_PathCarrot::CtrlTarget_PathCarrot(const arr& path, double maxDistance, const arr& times)
  : maxStep(maxDistance), endTime(times.last()), time(0.) {
  spline.set(2, path, times);
}

ActStatus CtrlTarget_PathCarrot::step(double tau, CtrlObjective* o, const ConfigurationL& Ctuple) {
  if(time+tau > endTime) tau = endTime - time;

  arr y_real, y_raw;
  getRealValue(o, y_raw, y_real, Ctuple);

  arr ref = spline.eval(time);
  arr goal = spline.eval(time + tau);
  double d1 = length(y_raw-ref);
  double d2 = length(ref-goal);

  if(d1 > maxStep) {
    //don't move foreward at all!
    goal = ref;
    tau = 0.;
    isTransient=true;
    countInRange=0;
  } else if(d1+d2 > maxStep) {
    tau *= (maxStep-d1)/d2;
    goal = spline.eval(time + tau);
    isTransient=true;
    countInRange=0;
  } else {
    isTransient=false;
    countInRange++;
  }
//  cout <<"  tau:" <<tau <<endl;
  time += tau;
  o->feat->target = goal;

  if(time>=endTime && countInRange>10) return AS_converged;
  return AS_running;
}

//===========================================================================

ActStatus CtrlTarget_Sine:: step(double tau, CtrlObjective* o, const ConfigurationL& Ctuple) {
  t+=tau;
  if(t>T) t=T;
  arr y_real =  o->feat->phi(Ctuple);
  if(y_start.N!=y_real.N) y_start=y_real; //initialization
  if(y_target.N!=y_real.N) y_target = y_start;
  o->feat->target = y_start + (.5*(1.-cos(RAI_PI*t/T))) * (y_target - y_start);
  y_err = o->feat->target - y_real;
  if(t>=T-1e-6/* && length(y_err)<1e-3*/) return AS_done;
//  return t>=T && length(y_err)<1e-3;
  return AS_running;
}

//===========================================================================

CtrlTarget_Bang::CtrlTarget_Bang(const arr& _y_target, double _maxVel)
  : y_target(_y_target), maxVel(_maxVel), tolerance(1e-3) {
}

void getAcc_bang(double& x, double& v, double maxVel, double tau) {
  double bang = maxVel/.1;

  if(fabs(x)<.1*maxVel) {
    v=0.;
    x=0.;
    return;
  }

  if(v<0.) {
    x*=-1.; v*=-1;
    getAcc_bang(x, v, maxVel, tau);
    x*=-1.; v*=-1;
    return;
  }

  if(x>0.) {
    v -= tau*bang;
    x += tau*v;
    if(x<0.) { x=0.; v=0.; }
    return;
  }

  double ahit = -0.5*(v*v)/x;

  //accelerate
  if(ahit<bang) {
    v += tau*bang;
    if(v>maxVel) v=maxVel;
    x += tau*v;
    return;
  }

  //deccelerate
  v -= tau*ahit;
  if(v<0.) v=0.;
}

void getVel_bang(double& x, double& v, double maxVel, double tau) {
  double sign=rai::sign(x); //-1=left
  v = -sign*maxVel;
  if((x<0. && x+tau*v > 0.) ||
      (x>0. && x+tau*v < 0.)) {
    v=0.; x=0;
  } else {
    x=x+tau*v;
  }
}

ActStatus CtrlTarget_Bang::step(double tau, CtrlObjective* o, const ConfigurationL& Ctuple) {
  //only on initialization the true state is used; otherwise ignored!
//  if(y_target.N!=y_real.N) { y_target=y_real; }

#if 0
#if 1
  target = y_real - y_target;
  arr v_ref = v_real;
  for(uint i=0; i<y_real.N; i++) {
    getAcc_bang(target(i), v_ref(i), maxVel, tau);
    if(i==2) {
      cout <<y_real(i) <<' ' <</*v_real(i) <<' '*/ <<v_ref(i) <<endl;
    }
  }
  target += y_target;
#else
  arr yDelta = y - y_target;
  v_ref.resizeAs(y).setZero();
  for(uint i=0; i<y.N; i++) getVel_bang(yDelta(i), v_ref(i), maxVel, tau);
  target = y_target + yDelta;
#endif

  if(maxDiff(y_real, y_target)<tolerance
      && absMax(v_real)<tolerance) {
    return AS_converged;
  }
#else
  NIY
#endif
  return AS_running;
}

//===========================================================================

CtrlTarget_PD::CtrlTarget_PD()
  : kp(0.), kd(0.), maxVel(-1.), maxAcc(-1.), flipTargetSignOnNegScalarProduct(false), makeTargetModulo2PI(false), tolerance(1e-3) {}

CtrlTarget_PD::CtrlTarget_PD(const arr& _y_target, double decayTime, double dampingRatio, double maxVel, double maxAcc)
  : CtrlTarget_PD() {
  y_target = _y_target;
  //    CHECK(decayTime>0. && dampingRatio>0., "this does not define proper gains!");
  //    double lambda = -decayTime*dampingRatio/log(.1);
  //    kp = rai::sqr(1./lambda);
  //    kd = 2.*dampingRatio/lambda;
  setGainsAsNatural(decayTime, dampingRatio);
}

CtrlTarget_PD::CtrlTarget_PD(const rai::Graph& params)
  : CtrlTarget_PD() {
  rai::Node* it;
  if((it=params["PD"])) {
    arr pd=it->get<arr>();
    setGainsAsNatural(pd(0), pd(1));
    maxVel = pd(2);
    maxAcc = pd(3);
  }
  if((it=params["target"])) y_target = it->get<arr>();
}

void CtrlTarget_PD::setGains(double _kp, double _kd) {
  kp = _kp;
  kd = _kd;
}

void CtrlTarget_PD::setGainsAsNatural(double decayTime, double dampingRatio) {
  naturalGains(kp, kd, decayTime, dampingRatio);
//  CHECK(decayTime>0. && dampingRatio>0., "this does not define proper gains!");
//  double lambda = -decayTime*dampingRatio/log(.1);
//  setGains(rai::sqr(1./lambda), 2.*dampingRatio/lambda);
}

ActStatus CtrlTarget_PD::step(double tau, CtrlObjective* o, const ConfigurationL& Ctuple) {
  //only on initialization the true state is used; otherwise ignored!
  arr y_real =  o->feat->phi(Ctuple);
  if(y_ref.N!=y_real.N) { y_ref=y_real; v_ref=zeros(y_real.N); }
  if(y_target.N!=y_ref.N) { y_target=y_ref; v_target=v_ref; }

  if(flipTargetSignOnNegScalarProduct && scalarProduct(y_target, y_ref) < 0) {
    y_target = -y_target;
  }
  if(makeTargetModulo2PI) for(uint i=0; i<y_ref.N; i++) {
      while(y_target(i) < y_ref(i)-RAI_PI) y_target(i)+=RAI_2PI;
      while(y_target(i) > y_ref(i)+RAI_PI) y_target(i)-=RAI_2PI;
    }

  arr a = getDesiredAcceleration();

  y_ref += tau*v_ref + (.5*tau*tau)*a;
  v_ref += tau*a;

  o->feat->target = y_ref;

  if(isConverged(-1.)) return AS_converged;
  return AS_running;
}

arr CtrlTarget_PD::getDesiredAcceleration() {
  arr a = kp*(y_target-y_ref) + kd*(v_target-v_ref);

  //check vel/acc limits
  double accNorm = length(a);
  if(accNorm>1e-4) {
    if(maxAcc>0. && accNorm>maxAcc) a *= maxAcc/accNorm;
    if(maxVel>0.) {
      double velRatio = scalarProduct(v_ref, a/accNorm)/maxVel;
      if(velRatio>1.) a.setZero();
      else if(velRatio>.9) a *= 1.-10.*(velRatio-.9);
    }
  }
  return a;
}

//arr CtrlTarget_PD::getDesiredAcceleration(){
//  arr Kp_y = Kp;
//  arr Kd_y = Kd;
//  makeGainsMatrices(Kp_y, Kd_y, y.N);
//  arr a = Kp_y*(get_y_ref()-y) + Kd_y*(get_ydot_ref()-v);

//  //check vel/acc limits
//  double accNorm = length(a);
//  if(accNorm<1e-4) return a;
//  if(maxAcc>0. && accNorm>maxAcc) a *= maxAcc/accNorm;
//  if(!maxVel) return a;
//  double velRatio = scalarProduct(v, a/accNorm)/maxVel;
//  if(velRatio>1.) a.setZero();
//  else if(velRatio>.9) a *= 1.-10.*(velRatio-.9);
//  return a;
//}

void CtrlTarget_PD::getDesiredLinAccLaw(arr& Kp_y, arr& Kd_y, arr& a0_y) {
  //this one doesn't depend on the current state...
  Kp_y = diag(kp, y_ref.N);
  Kd_y = diag(kd, y_ref.N);
  a0_y = Kp_y*y_target + Kd_y*v_target;
//  arr a = a0_y - Kp_y*y - Kd_y*v; //linear law
}

double CtrlTarget_PD::error() {
  if(!(y_ref.N && y_ref.N==y_target.N && v_ref.N==v_target.N)) return -1.;
  return maxDiff(y_ref, y_target) + maxDiff(v_ref, v_target);
}

bool CtrlTarget_PD::isConverged(double _tolerance) {
  if(_tolerance<0.) _tolerance=tolerance;
  return (y_ref.N && y_ref.N==y_target.N && v_ref.N==v_target.N
          && maxDiff(y_ref, y_target)<_tolerance
          && maxDiff(v_ref, v_target)<_tolerance); //TODO what if Kp = 0, then it should not count?!?
}

