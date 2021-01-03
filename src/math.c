/*
 math.c
 */

typedef struct {
  int32_t x;
  int32_t y;
} iVEC;
typedef struct {
  double x;
  double y;
} VEC;

double VEC_len(VEC v) {
  return sqrt(pow(v.x, 2) + pow(v.y, 2));

}

VEC VEC_add(VEC v1, VEC v2) {
  VEC result = { v1.x + v2.x, v1.y + v2.y };
  return result;
}
VEC VEC_sub(VEC v1, VEC v2) {
  VEC result = { v1.x - v2.x, v1.y - v2.y };
  return result;
}
VEC VEC_scale(VEC v, double s) {
  VEC result = { v.x * s, v.y * s };
  return result;
}

VEC VEC_neg(VEC v) {
  return VEC_scale(v, -1);
}

double VEC_dot(VEC v1, VEC v2) {
  return v1.x * v2.x + v1.y * v2.y;
}

VEC VEC_perp(VEC v) {
  VEC result = { -v.y , v.x };
  return result;
}




int64_t max(int64_t n1, int64_t n2) {
  if (n1 > n2) {
    return n1;
  }
  return n2;
}

int64_t min(int64_t n1, int64_t n2) {
  if (n1 < n2) {
    return n1;
  }
  return n2;
}

double fmid(double n1, double n2, double n3) {
  double temp;
  if (n1 > n3) {
    temp = n1;
    n1 = n3;
    n3 = temp;
  }
  if (n1 > n2) {
    temp = n1;
    n1 = n2;
    n2 = temp;
  }
  if (n2 < n3) {
    return n2;
  } else {
    return n3;
  }
}

int64_t mid(int64_t n1, int64_t n2, int64_t n3) {
  int64_t temp;
  if (n1 > n3) {
    temp = n1;
    n1 = n3;
    n3 = temp;
  }
  if (n1 > n2) {
    temp = n1;
    n1 = n2;
    n2 = temp;
  }
  if (n2 < n3) {
    return n2;
  } else {
    return n3;
  }
}

internal uint64_t
gcd(uint64_t a, uint64_t b) {
  if (a == b) {
    return a;
  }
  if (a == 0) {
    return b;
  }
  if (b == 0) {
    return a;
  }
  if ((a % 2) == 0) {
    // a is even
    if ((b % 2) == 1) {
      // b is odd
      return gcd(a/2, b);
    } else {
      // b is even
      return gcd(a/2, b/2);
    }
  } else {
    // a is odd
    if ((b % 2) == 0) {
      // b is even
      return gcd(a, b/2);
    }
    if (a > b) {
      // a and b are both odd
      return gcd((a - b)/2, b);
    } else {
      return gcd((b - a)/2, a);
    }
  }
}
