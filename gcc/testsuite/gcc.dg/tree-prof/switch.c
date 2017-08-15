/* { dg-options "-O2 -fdump-ipa-profile --param min-switch-case-hotness=1" } */

int counter = 0;

void
foo (int value)
{
  switch (value)
    {
    case 44 ... 46:
      counter += 46;
      break;
    case 93 ... 94:
      counter += 46;
      break;
    case 166:
      counter += 46;
      break;
    case 225:
      break;
    case 250 ... 256:
      counter += 46;
      break;
    case 330 ... 333:
      counter += 46;
      break;
    case 383:
      counter += 46;
      break;
    case 434:
      counter += 46;
      break;
    case 480 ... 488:
      counter += 488;
      break;
    case 560:
      counter += 46;
      break;
    case 570 ... 579:
      counter += 560;
      break;
    case 626:
      break;
    case 685:
      break;
    case 727:
      counter += 46;
      break;
    case 731 ... 734:
      counter += 560;
      break;
    case 801:
      counter += 727;
      break;
    case 829 ... 836:
      counter += 560;
      break;
    case 895 ... 899:
      counter += 836;
      break;
    case 922 ... 928:
      counter += 488;
      break;
    case 975:
      counter += 560;
      break;
    case 979:
      break;
    case 984 ... 990:
      counter += 727;
      break;
    case 1080 ... 1086:
      break;
    case 1182 ... 1183:
      break;
    default:
      counter += 9614;
      break;
    }
}
int
main ()
{
  const int sample[] = {1080, 1080, 1080, 1080, 111, 922};
  for (unsigned i = 0; i < 6; i++)
    {
      foo (sample[i]);
    }
  __builtin_printf ("counter: %d\n", counter);

  return 0;
}

/* { dg-final-use-not-autofdo { scan-ipa-dump-times "Adding case comparsion before switch statement: case 922 ... 928: with probability: 16.67%" 1 "profile"} } */
/* { dg-final-use-not-autofdo { scan-ipa-dump-times "Adding case comparsion before switch statement: case 1080 ... 1086: with probability: 66.67%" 1 "profile"} } */
