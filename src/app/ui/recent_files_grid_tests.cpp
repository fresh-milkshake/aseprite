// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#include "app/ui/recent_files_grid.h"
#include "tests/app_test.h"

using namespace app;

TEST(RecentFilesGrid, NarrowViewHasNoHorizontalOverflow)
{
  const auto grid = recent_grid_layout(70, 4, 1);
  EXPECT_EQ(1, grid.columns);
  EXPECT_EQ(4, grid.rows);
  EXPECT_EQ(70, grid.cardWidth);
  EXPECT_EQ(0, grid.itemBounds(3).x);
  EXPECT_EQ(grid.height, grid.itemBounds(3).y2());
}

TEST(RecentFilesGrid, ReflowsAtColumnBoundary)
{
  EXPECT_EQ(1, recent_grid_layout(409, 5, 1).columns);
  const auto grid = recent_grid_layout(410, 5, 1);
  EXPECT_EQ(2, grid.columns);
  EXPECT_EQ(3, grid.rows);
  EXPECT_EQ(0, grid.itemBounds(4).x);
  EXPECT_LE(grid.itemBounds(1).x2(), 410);
  EXPECT_EQ(grid.height, grid.itemBounds(4).y2());
}

TEST(RecentFilesGrid, ScalesAndFillsAvailableWidth)
{
  const auto normal = recent_grid_layout(600, 7, 1);
  const auto scaled = recent_grid_layout(1200, 7, 2);
  EXPECT_EQ(normal.columns, scaled.columns);
  EXPECT_EQ(normal.rows, scaled.rows);
  EXPECT_EQ(normal.cardWidth * 2, scaled.cardWidth);
  EXPECT_EQ(normal.cardHeight * 2, scaled.cardHeight);
  EXPECT_EQ(normal.height * 2, scaled.height);
  EXPECT_LE(normal.itemBounds(normal.columns - 1).x2(), 600);
}

TEST(RecentFilesGrid, EmptyAndUninitializedViewsHaveValidGeometry)
{
  const auto empty = recent_grid_layout(0, 0, 1);
  EXPECT_EQ(1, empty.columns);
  EXPECT_EQ(0, empty.rows);
  EXPECT_GT(empty.cardWidth, 0);
  EXPECT_GT(empty.height, 0);
}

TEST(RecentFilesGrid, SquarePreviewFitsWideCardWithoutPaintingLabels)
{
  const gfx::Rect area(20, 30, 232, 64);
  const auto image = fit_recent_preview(gfx::Size(16, 16), area);
  EXPECT_EQ(gfx::Rect(104, 30, 64, 64), image);
  EXPECT_TRUE(area.contains(image));
}

TEST(RecentFilesGrid, WideAndTallPreviewsRespectBothBounds)
{
  const gfx::Rect area(7, 11, 90, 60);
  const auto landscape = fit_recent_preview(gfx::Size(128, 16), area);
  const auto portrait = fit_recent_preview(gfx::Size(16, 128), area);
  EXPECT_TRUE(area.contains(landscape));
  EXPECT_TRUE(area.contains(portrait));
  EXPECT_EQ(90, landscape.w);
  EXPECT_EQ(60, portrait.h);
  EXPECT_TRUE(fit_recent_preview(gfx::Size(0, 0), area).isEmpty());
  EXPECT_TRUE(fit_recent_preview(gfx::Size(16, 16), gfx::Rect()).isEmpty());
}

TEST(RecentFilesGrid, SmallPixelArtUsesIntegerMagnification)
{
  const auto result = fit_recent_preview(gfx::Size(16, 16), gfx::Rect(0, 0, 101, 77));
  EXPECT_EQ(64, result.w);
  EXPECT_EQ(64, result.h);
}

TEST(RecentFilesGrid, InlineZoomKeepsThePointUnderThePointer)
{
  const gfx::Rect viewport(0, 0, 200, 100);
  const gfx::Rect fit(0, 0, 200, 100);
  EXPECT_EQ(gfx::Rect(-60, -30, 400, 200),
            zoom_recent_preview(fit, viewport, 2.0, gfx::Point(60, 30)));
  EXPECT_EQ(gfx::Rect(0, -100, 400, 200),
            zoom_recent_preview(gfx::Rect(15, -120, 400, 200), viewport, 1.0, gfx::Point()));
}
